// The supervisor: selects tests, hands them to worker processes (--worker <ids>), collects what the workers report and
// writes the reports. A worker that crashes, panics on a validation error or hangs only takes the test it was on down.
// People read Toolkit's log lines; tools read protocol events (--format events, protocol.hpp).

#include "options.hpp"
#include "protocol.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/filesystem/file.hpp"
#include "tkit/multiprocessing/task.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/utils/literals.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#    define popen _popen
#    define pclose _pclose
#else
#    include <sys/wait.h>
#endif

namespace OnyxTest
{
using namespace TKit::Literals;

// The supervisor never initializes Onyx: its strings come from the allocators PushThreadAllocator gives its threads,
// which serve allocations up to SupervisorMaxAllocation (options.hpp) and nothing larger. A worker can print without
// end, so what is kept of a test is bounded: lines are cut, only the end of its output stays and failures past a count
// are dropped (and said to be). Reports are written a piece at a time for the same reason: none is ever held whole in
// memory.
constexpr usize MaxLineSize = usize(16_kib);
constexpr usize MaxOutputSize = usize(64_kib);
constexpr u32 MaxFailures = 64;
constexpr u32 MaxNotes = 64;
constexpr u32 MaxOutputLinesShown = 80;

struct Failure
{
    String Where;
    String Expression;
    String Expansion;
    String Info;
};

struct Result
{
    const TestInfo *Test = nullptr;
    Status State = Status_Crash;
    f64 Seconds = 0.0;
    String SkipReason{};
    String ExitInfo{};
    Array<Failure> Failures{};
    Array<String> Notes{};
    // the end of what the test printed, a line after another, shown for failures
    String Output{};
    u32 DroppedFailures = 0;
};

static constexpr u32 MaxJobs = 64;

// a line as text, without the terminal's color codes (Toolkit colors its log levels) or other control characters.
// they have no glyph in the control center's font, and they are not valid XML 1.0, which the JUnit report is
static String plainText(const String &line)
{
    String out{};
    for (usize i = 0; i < line.GetSize(); ++i)
    {
        const char c = line[i];
        if (c == '\x1b')
        {
            // CSI: ESC [ parameters, then a final byte in @ to ~. a lone ESC is dropped by itself
            if (i + 1 < line.GetSize() && line[i + 1] == '[')
            {
                i += 2;
                while (i < line.GetSize() && (line[i] < '@' || line[i] > '~'))
                    ++i;
            }
            continue;
        }
        if (u8(c) < 0x20 && c != '\t')
            continue;
        out += c;
    }
    return out;
}

// a failure as people read it, a line at a time: where and what, what Catch2 expanded it to, the messages in force
template <typename F> static void describe(const Failure &f, F &&line)
{
    if (f.Expression.IsEmpty())
        line(StringView{f.Where});
    else
        line(StringView{String::Format("{}: {}", f.Where, f.Expression)});
    if (!f.Expansion.IsEmpty() && f.Expansion != f.Expression)
        ForEachLine(f.Expansion,
                    [&](const StringView part) { line(StringView{String::Format("    with expansion: {}", part)}); });
    ForEachLine(f.Info, [&](const StringView part) { line(StringView{String::Format("    {}", part)}); });
}
static String firstLine(const Failure &f)
{
    return f.Expression.IsEmpty() ? f.Where : String::Format("{}: {}", f.Where, f.Expression);
}

// everything a result has to say besides its output, a line at a time
template <typename F> static void details(const Result &result, F &&line)
{
    for (const Failure &f : result.Failures)
        describe(f, line);
    if (result.DroppedFailures != 0)
        line(StringView{String::Format("and {} more failures, not kept", result.DroppedFailures)});
    if (!result.ExitInfo.IsEmpty())
        line(StringView{result.ExitInfo});
    for (const String &note : result.Notes)
        line(StringView{String::Format("note: {}", note)});
}

// the last lines of a text, at most count of them
static StringView lastLines(const StringView text, const u32 count)
{
    usize start = text.GetSize();
    // the newline that ends the text does not start a line
    if (start != 0 && text[start - 1] == '\n')
        --start;
    u32 lines = 0;
    while (start != 0)
    {
        if (text[start - 1] == '\n' && ++lines == count)
            break;
        --start;
    }
    return StringView{text.GetData() + start, usize(text.GetSize() - start)};
}
static u32 countLines(const StringView text)
{
    u32 lines = 0;
    ForEachLine(text, [&lines](StringView) { ++lines; });
    return lines;
}

static bool isBad(const Status status)
{
    return status == Status_Fail || status == Status_Crash || status == Status_Timeout;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Selection
// ---------------------------------------------------------------------------------------------------------------------

static bool matches(const char *pattern, const char *text)
{
    if (*pattern == '\0')
        return *text == '\0';
    if (*pattern == '*')
        return matches(pattern + 1, text) || (*text != '\0' && matches(pattern, text + 1));
    if (*text != '\0' && (*pattern == '?' || *pattern == *text))
        return matches(pattern + 1, text + 1);
    return false;
}

static Array<const TestInfo *> selectTests(const Options &options)
{
    const auto hits = [](const String &pattern, const TestInfo &test) {
        return matches(pattern.CString(), test.Id) || pattern == test.Suite;
    };
    Array<const TestInfo *> selected{};
    for (const TestInfo *test = GetTests(); test; test = test->Next)
    {
        bool included = options.Include.IsEmpty();
        for (const String &pattern : options.Include)
            included |= hits(pattern, *test);
        for (const String &pattern : options.Exclude)
            included &= !hits(pattern, *test);
        if (included)
            selected.Append(test);
    }
    // suites in alphabetical order, tests in the order they are written in
    for (usize i = 1; i < selected.GetSize(); ++i)
        for (usize j = i; j > 0 && std::strcmp(selected[j]->Suite, selected[j - 1]->Suite) < 0; --j)
        {
            const TestInfo *tmp = selected[j];
            selected[j] = selected[j - 1];
            selected[j - 1] = tmp;
        }
    return selected;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Running a worker
// ---------------------------------------------------------------------------------------------------------------------

static String workerCommand(const Options &options, const String &tests)
{
    String command = String::Format("\"{}\" --worker {} --speed {} --timeout-scale {} 2>&1", options.Self, tests,
                                    SpeedNames[options.PlaySpeed], options.TimeoutScale);
#ifdef _WIN32
    // cmd /c strips the outer quotes of the whole line: wrap it once more so the quoted executable path survives
    command = "\"" + command + "\"";
#endif
    return command;
}

// lines Catch2 prints on its way out of a failed REQUIRE without exceptions: they explain nothing the failure does not
static bool isCatchNoise(const String &line)
{
    return line.StartsWith("Catch will terminate because it needed to throw an exception") ||
           line.StartsWith("The message was: ");
}

// keeps the end of what a test printed: once it is over its size, the older half goes, up to the end of a line
static void appendOutput(Result &result, const String &line)
{
    if (result.Output.GetSize() + line.GetSize() + 1 > MaxOutputSize)
    {
        const usize half = MaxOutputSize / 2;
        const usize from = result.Output.GetSize() > half ? result.Output.GetSize() - half : 0;
        const usize at = result.Output.Find('\n', from);
        const String kept = at == String::npos ? String{} : result.Output.SubString(at + 1);
        result.Output = "[... earlier output dropped ...]\n";
        result.Output += kept;
    }
    result.Output += line;
    result.Output += '\n';
}

// The tests a worker process is given. Tests that want the same of Onyx share a worker, and Onyx with it: starting
// Onyx takes far longer than most tests do. A worker ends at the first test that does not pass, so tests after it may
// never start: those go back to the queue.
//
// Sharing must not change a verdict. The first test of a worker starts from a fresh Onyx, as does a test run alone, so
// what they report stands. A later test that fails, crashes or times out may only have tripped over what the tests
// before it left behind: it runs again, alone, and that run decides. If it passes there, it passes, with a note that
// says so (one of the tests before it leaves something behind).
constexpr u32 MaxBatchSize = 24;
struct Batch
{
    TKit::StaticArray<u32, MaxBatchSize> Tests{};
    bool Alone = false;
};

// where a test is on its way to a verdict
enum Stage : u8
{
    Stage_Queued,
    // to run in a worker of its own, after it went wrong in one it shared
    Stage_QueuedAlone,
    Stage_Running,
    Stage_Done,
};

struct Run
{
    const Options &Opts;
    Array<const TestInfo *> Tests{};
    // the verdicts. a result is written by the job thread that runs its test and by no other (allocators are per
    // thread), and read by the others only once it is reported
    Array<Result> Results{};
    // what a test did in a worker it shared, when it went wrong there and runs again alone: kept to tell
    Array<Result> Shared{};
    Array<bool> Retried{};
    Array<bool> DuringTeardown{};

    std::mutex Mutex{};
    std::condition_variable Changed{};
    // guarded by Mutex
    Array<Stage> Stages{};
    u32 BatchSize = 1;
    u32 Completed = 0;

    explicit Run(const Options &options) : Opts(options)
    {
    }
};

static bool runsAlone(const Run &run, const u32 index)
{
    const TestOptions &options = run.Tests[index]->Options;
    // a known issue is expected to end its worker
    return run.Opts.Isolate || options.OwnProcess || options.KnownIssue;
}

// the next tests to run. false when every test is done. an empty batch means there is nothing to start right now, but
// tests that are running may still come back to the queue
static bool nextBatch(Run &run, Batch &batch)
{
    std::unique_lock lock{run.Mutex};
    for (;;)
    {
        batch.Tests.Clear();
        batch.Alone = false;
        bool running = false;
        for (u32 i = 0; i < run.Stages.GetSize(); ++i)
        {
            running |= run.Stages[i] == Stage_Running;
            if (run.Stages[i] != Stage_QueuedAlone)
                continue;
            batch.Tests.Append(i);
            batch.Alone = true;
            break;
        }
        for (u32 i = 0; i < run.Stages.GetSize() && !batch.Alone && batch.Tests.GetSize() < run.BatchSize; ++i)
        {
            if (run.Stages[i] != Stage_Queued)
                continue;
            if (batch.Tests.IsEmpty())
            {
                batch.Tests.Append(i);
                batch.Alone = runsAlone(run, i);
            }
            else if (!runsAlone(run, i) &&
                     run.Tests[i]->Options.Init == run.Tests[batch.Tests.GetFront()]->Options.Init)
                batch.Tests.Append(i);
        }
        if (!batch.Tests.IsEmpty())
        {
            for (const u32 i : batch.Tests)
                run.Stages[i] = Stage_Running;
            return true;
        }
        if (!running)
            return false;
        run.Changed.wait(lock);
    }
}

static void reportResult(const Options &options, const Result &result, u32 index, u32 total);

// a test's verdict is in: known issues, the note of a test that only passed alone, and the report
static void finishTest(Run &run, const u32 index)
{
    Result &result = run.Results[index];
    const TestInfo &test = *result.Test;
    const Result &shared = run.Shared[index];
    if (run.Retried[index] && !isBad(result.State))
    {
        String what = String::Format(
            "Passed alone, but {} in a worker it shared with the tests before it: one of them leaves something "
            "behind that this test, or Onyx, does not cope with. Run them with --isolate to find which.",
            run.DuringTeardown[index]
                ? "Onyx::Terminate crashed after it"
                : (shared.State == Status_Fail ? "failed"
                                               : (shared.State == Status_Timeout ? "timed out" : "crashed")));
        if (!shared.Failures.IsEmpty())
            what += " There it said: " + firstLine(shared.Failures[0]);
        if (!shared.ExitInfo.IsEmpty())
            what += " There it ended with: " + shared.ExitInfo;
        // an assert says more than the signal it ends with
        StringView panic{};
        ForEachLine(shared.Output, [&panic](const StringView line) {
            if (line.Contains("[PANIC]"))
                panic = line;
        });
        const StringView lastPanic = panic;
        if (!lastPanic.IsEmpty())
            what += " Its last panic: " + String{lastPanic};
        result.Notes.Append(what);
    }
    if (const char *issue = test.Options.KnownIssue)
    {
        if (isBad(result.State))
        {
            result.Notes.Append(String::Format("Known issue ({}): {}", StatusNames[result.State], issue));
            result.State = Status_Known;
        }
        else if (result.State == Status_Pass)
            result.Notes.Append(
                String::Format("The known issue did not reproduce. If it is fixed, remove KnownIssue: {}", issue));
    }
    // only failures show their output, unless asked for all of it
    if (!isBad(result.State) && !run.Opts.Verbose)
        result.Output = String{};

    u32 completed = 0;
    {
        const std::lock_guard lock{run.Mutex};
        completed = run.Completed++;
    }
    reportResult(run.Opts, result, completed, u32(run.Tests.GetSize()));
}

// runs the tests of a batch in one worker process, as far as it gets, and sends each of them on its way
static void runBatch(Run &run, const Batch &batch)
{
    const Options &options = run.Opts;
    String ids{};
    for (const u32 i : batch.Tests)
    {
        if (!ids.IsEmpty())
            ids += ',';
        ids += run.Tests[i]->Id;
    }

    // what the worker did with each test of the batch
    enum Progress : u8
    {
        Progress_NotStarted,
        Progress_Started,
        Progress_Ended,
    };
    TKit::StaticArray<Progress, MaxBatchSize> progress{};
    progress.Resize(batch.Tests.GetSize());
    for (Progress &p : progress)
        p = Progress_NotStarted;

    // the test the worker is on, and the first one it started: that one found Onyx fresh
    u32 current = TKIT_U32_MAX;
    u32 first = TKIT_U32_MAX;
    u32 last = TKIT_U32_MAX;
    bool teardown = false;
    bool done = false;
    TKit::Clock clock{};
    // what is printed before a test starts goes with the next one
    Result preamble{};

    const auto result = [&](const u32 slot) -> Result & {
        const u32 index = batch.Tests[slot];
        return (batch.Alone || slot == first) ? run.Results[index] : run.Shared[index];
    };
    const auto readLine = [&](const String &line) {
        Array<String> fields{};
        if (!ParseEvent(line, fields))
        {
            // blank lines only push what matters out of the lines kept
            if (!line.IsEmpty() && !isCatchNoise(line))
                appendOutput(current == TKIT_U32_MAX ? preamble : result(current), plainText(line));
            return;
        }
        const String &kind = fields[0];
        if (kind == "begin" && fields.GetSize() >= 2)
        {
            for (u32 slot = 0; slot < batch.Tests.GetSize(); ++slot)
                if (fields[1] == run.Tests[batch.Tests[slot]]->Id)
                    current = slot;
            if (current == TKIT_U32_MAX)
                return;
            if (first == TKIT_U32_MAX)
                first = current;
            last = current;
            progress[current] = Progress_Started;
            clock.Restart();
            Result &r = result(current);
            r.Test = run.Tests[batch.Tests[current]];
            r.State = Status_Crash;
            r.Output = std::move(preamble.Output);
            preamble.Output = String{};
            if (options.Output == Output_Events)
                Emit("begin", r.Test->Id);
            return;
        }
        if (kind == "teardown")
            teardown = true;
        else if (kind == "done")
            done = true;
        if (current == TKIT_U32_MAX)
            return;
        Result &r = result(current);
        if (kind == "fail" && fields.GetSize() >= 5)
        {
            if (r.Failures.GetSize() < MaxFailures)
                r.Failures.Append(Failure{fields[1], fields[2], fields[3], fields[4]});
            else
                ++r.DroppedFailures;
        }
        else if (kind == "skip" && fields.GetSize() >= 2)
            r.SkipReason = fields[1];
        else if (kind == "note" && fields.GetSize() >= 2)
        {
            if (r.Notes.GetSize() < MaxNotes)
                r.Notes.Append(fields[1]);
        }
        else if (kind == "end" && fields.GetSize() >= 3)
        {
            for (u32 i = 0; i < Status_Count; ++i)
                if (fields[2] == StatusNames[i])
                    r.State = Status(i);
            r.Seconds = clock.GetElapsed().AsSeconds<f64>();
            progress[current] = Progress_Ended;
            current = TKIT_U32_MAX;
        }
    };

    int code = 0;
    std::FILE *pipe = popen(workerCommand(options, ids).CString(), "r");
    if (pipe)
    {
        String line{};
        // a line longer than MaxLineSize is cut there: the rest of it is read and dropped
        bool cut = false;
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe))
        {
            const usz length = std::strlen(buffer);
            const bool whole = length != 0 && buffer[length - 1] == '\n';
            if (!cut)
            {
                line += buffer;
                if (line.GetSize() > MaxLineSize)
                {
                    line = line.SubString(0, MaxLineSize);
                    line += " [... cut]";
                    cut = true;
                }
            }
            if (!whole)
                continue;
            while (!line.IsEmpty() && (line.GetBack() == '\n' || line.GetBack() == '\r'))
                line.Pop();
            readLine(line);
            line.Clear();
            cut = false;
        }
        // a worker that dies half way through a line still said something
        if (!line.IsEmpty())
            readLine(line);
        code = pclose(pipe);
    }

    String exitInfo{};
    if (!pipe)
        exitInfo = "Could not start the worker process";
#ifdef _WIN32
    else if (code != 0)
        exitInfo = String::Format("Exit code {}", code);
#else
    else if (WIFSIGNALED(code))
        exitInfo = String::Format("Killed by signal {}", WTERMSIG(code));
    else if (WIFEXITED(code) && WEXITSTATUS(code) != 0)
        exitInfo = String::Format("Exit code {}", WEXITSTATUS(code));
#endif

    // a worker always gets somewhere: if it started nothing, its first test takes the blame, and the rest another try
    if (first == TKIT_U32_MAX)
    {
        first = last = current = 0;
        progress[0] = Progress_Started;
        Result &r = result(0);
        r.Test = run.Tests[batch.Tests[0]];
        r.State = Status_Crash;
        r.Output = std::move(preamble.Output);
        if (exitInfo.IsEmpty())
            exitInfo = "The worker ended before it started a test";
    }
    if (current != TKIT_U32_MAX)
    {
        // it never said how it ended
        Result &r = result(current);
        r.State = Status_Crash;
        r.Seconds = clock.GetElapsed().AsSeconds<f64>();
        r.ExitInfo = exitInfo.IsEmpty() ? String{"The worker ended without reporting"} : exitInfo;
    }
    else if (!done && !exitInfo.IsEmpty())
    {
        // every test it started said how it ended, and then the worker did not end well: the last one lost something
        Result &r = result(last);
        if (r.State == Status_Pass)
        {
            r.State = Status_Crash;
            r.ExitInfo = String::Format("Reported a pass, but its worker did not end well: {}", exitInfo);
        }
    }

    for (u32 slot = 0; slot < batch.Tests.GetSize(); ++slot)
    {
        const u32 index = batch.Tests[slot];
        Stage stage = Stage_Queued;
        if (progress[slot] != Progress_NotStarted)
        {
            const bool stands = batch.Alone || slot == first;
            if (!stands && isBad(run.Shared[index].State))
            {
                run.Retried[index] = true;
                run.DuringTeardown[index] = teardown && slot == last;
                stage = Stage_QueuedAlone;
            }
            else
            {
                if (!stands)
                    // a pass in good company is a pass. moved by the thread that filled it
                    run.Results[index] = std::move(run.Shared[index]);
                finishTest(run, index);
                stage = Stage_Done;
            }
        }
        const std::lock_guard lock{run.Mutex};
        run.Stages[index] = stage;
    }
    run.Changed.notify_all();
}

// ---------------------------------------------------------------------------------------------------------------------
//  Reporting as tests finish
// ---------------------------------------------------------------------------------------------------------------------

static const char *upperStatus(const Status status)
{
    constexpr const char *names[Status_Count] = {"PASS", "FAIL", "SKIP", "CRASH", "TIMEOUT", "KNOWN"};
    return names[status];
}

// a line in the level that fits the outcome: passes as info, skips and known issues as warnings, the rest as errors
static void logLine(const Status status, const String &text)
{
    // braces: the log macros are if statements themselves
    if (status == Status_Pass)
    {
        TKIT_LOG_INFO("{}", text);
    }
    else if (status == Status_Skip || status == Status_Known)
    {
        TKIT_LOG_WARNING("{}", text);
    }
    else
    {
        TKIT_LOG_ERROR("{}", text);
    }
}

static std::mutex s_ReportMutex{};

static void reportResult(const Options &options, const Result &result, const u32 index, const u32 total)
{
    const TestInfo &test = *result.Test;
    const std::lock_guard lock{s_ReportMutex};
    if (options.Output == Output_Events)
    {
        for (const Failure &f : result.Failures)
            Emit("fail", test.Id, f.Where, f.Expression, f.Expansion, f.Info);
        if (!result.SkipReason.IsEmpty())
            Emit("skip", test.Id, result.SkipReason);
        for (const String &note : result.Notes)
            Emit("note", test.Id, note);
        if (result.DroppedFailures != 0)
            Emit("note", test.Id, String::Format("{} more failures were not kept", result.DroppedFailures));
        if (!result.ExitInfo.IsEmpty())
            Emit("crash", test.Id, result.ExitInfo);
        ForEachLine(result.Output, [&test](const StringView line) { Emit("output", test.Id, line); });
        Emit("end", test.Id, StatusNames[result.State], String::Format("{}", u64(result.Seconds * 1000.0)));
        return;
    }

    const String count = String::Format("{}", total);
    String headline = String::Format("[ONYX-TESTS] [{:>{}}/{}] {:<7} {} ({:.0f} ms)", index + 1, count.GetSize(), total,
                                     upperStatus(result.State), test.Id, result.Seconds * 1000.0);
    if (result.State == Status_Skip && !result.SkipReason.IsEmpty())
        headline += ": " + result.SkipReason;
    logLine(result.State, headline);
    details(result, [&result](const StringView line) { logLine(result.State, String::Format("    {}", line)); });
    if (!result.Output.IsEmpty())
    {
        // in the order they were printed: the last line is the one closest to what went wrong
        const StringView shown = lastLines(result.Output, MaxOutputLinesShown);
        logLine(result.State, String::Format("    --- The last {} lines it printed ---", countLines(shown)));
        ForEachLine(shown,
                    [&result](const StringView line) { logLine(result.State, String::Format("    | {}", line)); });
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//  Reports: JUnit, Markdown summary, GitHub annotations
// ---------------------------------------------------------------------------------------------------------------------

// a report file, written a piece at a time
class Report
{
  public:
    Report(const Path &path, const TKit::FileOpenFlags flags)
    {
        auto file = TKit::File::Open(path, flags);
        if (file)
            m_File = std::move(*file);
        else
            TKIT_LOG_ERROR("[ONYX-TESTS] Could not write '{}': {}", path, TKit::ToString(file.GetError()));
    }

    void Write(const StringView text)
    {
        if (!m_File.IsOpen() || text.IsEmpty())
            return;
        const auto written = m_File.Write(text);
        if (written)
            return;
        TKIT_LOG_ERROR("[ONYX-TESTS] Could not finish a report: {}", TKit::ToString(written.GetError()));
        m_File.Close();
    }
    template <typename... Args> void Format(const fmt::format_string<Args...> format, Args &&...args)
    {
        Write(String::Format(format, std::forward<Args>(args)...));
    }

    // text inside an XML element or attribute
    void WriteXml(const StringView text)
    {
        writeEscaped(text, [](const char c) -> const char * {
            switch (c)
            {
            case '&':
                return "&amp;";
            case '<':
                return "&lt;";
            case '>':
                return "&gt;";
            case '"':
                return "&quot;";
            default:
                // control characters are not allowed in XML 1.0, and logs carry ANSI colors
                return (u8(c) >= 0x20 || c == '\n' || c == '\t') ? nullptr : "";
            }
        });
    }
    // text inside a cell of a Markdown table
    void WriteMarkdownCell(const StringView text)
    {
        writeEscaped(text, [](const char c) -> const char * {
            if (c == '|')
                return "\\|";
            if (c == '\n')
                return "<br>";
            return u8(c) >= 0x20 ? nullptr : "";
        });
    }

  private:
    // replacement gives what a character becomes, or null for the character itself
    template <typename F> void writeEscaped(const StringView text, F &&replacement)
    {
        char chunk[1024];
        usz size = 0;
        const auto put = [&](const char *data, const usz count) {
            if (size + count > sizeof(chunk))
            {
                Write(StringView{chunk, usize(size)});
                size = 0;
            }
            std::memcpy(chunk + size, data, count);
            size += count;
        };
        for (const char &c : text)
        {
            const char *other = replacement(c);
            if (other)
                put(other, std::strlen(other));
            else
                put(&c, 1);
        }
        Write(StringView{chunk, usize(size)});
    }

    TKit::File m_File{};
};

static void writeJUnit(const Options &options, const Array<Result> &results, const f64 seconds)
{
    u32 counts[Status_Count]{};
    for (const Result &r : results)
        ++counts[r.State];

    Report xml{options.JUnit, TKit::FileOpenFlag_Overwrite};
    xml.Write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites name=\"onyx-tests");
    if (!options.Label.IsEmpty())
    {
        xml.Write(" ");
        xml.WriteXml(options.Label);
    }
    xml.Format("\" tests=\"{}\" failures=\"{}\" errors=\"{}\" skipped=\"{}\" time=\"{:.3f}\">\n", results.GetSize(),
               counts[Status_Fail], counts[Status_Crash] + counts[Status_Timeout],
               counts[Status_Skip] + counts[Status_Known], seconds);

    usize i = 0;
    while (i < results.GetSize())
    {
        const char *suite = results[i].Test->Suite;
        usize end = i;
        u32 suiteCounts[Status_Count]{};
        f64 suiteSeconds = 0.0;
        while (end < results.GetSize() && std::strcmp(suite, results[end].Test->Suite) == 0)
        {
            ++suiteCounts[results[end].State];
            suiteSeconds += results[end].Seconds;
            ++end;
        }
        xml.Format("  <testsuite name=\"{}\" tests=\"{}\" failures=\"{}\" errors=\"{}\" skipped=\"{}\" "
                   "time=\"{:.3f}\">\n",
                   suite, end - i, suiteCounts[Status_Fail], suiteCounts[Status_Crash] + suiteCounts[Status_Timeout],
                   suiteCounts[Status_Skip] + suiteCounts[Status_Known], suiteSeconds);
        for (; i < end; ++i)
        {
            const Result &r = results[i];
            xml.Format("    <testcase classname=\"{}\" name=\"{}\" file=\"", r.Test->Suite, r.Test->Name);
            xml.WriteXml(RelativePath(r.Test->File));
            xml.Format("\" line=\"{}\" time=\"{:.3f}\">", r.Test->Line, r.Seconds);

            const auto text = [&] {
                details(r, [&xml](const StringView line) {
                    xml.WriteXml(line);
                    xml.Write("\n");
                });
            };
            if (r.State == Status_Fail)
            {
                xml.Write("\n      <failure message=\"");
                xml.WriteXml(r.Failures.IsEmpty() ? String{"failed"} : firstLine(r.Failures[0]));
                xml.Write("\">");
                text();
                xml.Write("</failure>");
            }
            else if (r.State == Status_Crash || r.State == Status_Timeout)
            {
                xml.Format("\n      <error message=\"{}\">", StatusNames[r.State]);
                text();
                xml.Write("</error>");
            }
            else if (r.State == Status_Skip)
            {
                xml.Write("\n      <skipped message=\"");
                xml.WriteXml(r.SkipReason);
                xml.Write("\"/>");
            }
            else if (r.State == Status_Known)
            {
                xml.Write("\n      <skipped message=\"known issue: ");
                xml.WriteXml(StringView{r.Test->Options.KnownIssue});
                xml.Write("\">");
                text();
                xml.Write("</skipped>");
            }
            if (isBad(r.State) && !r.Output.IsEmpty())
            {
                xml.Write("\n      <system-out>");
                xml.WriteXml(r.Output);
                xml.Write("</system-out>");
            }
            xml.Write(r.State == Status_Pass ? "</testcase>\n" : "\n    </testcase>\n");
        }
        xml.Write("  </testsuite>\n");
    }
    xml.Write("</testsuites>\n");
}

static void writeSummary(const Options &options, const Array<Result> &results, const f64 seconds)
{
    u32 counts[Status_Count]{};
    for (const Result &r : results)
        ++counts[r.State];
    const u32 bad = counts[Status_Fail] + counts[Status_Crash] + counts[Status_Timeout];

    // appended: on GitHub several steps (one per configuration) share the summary file
    Report md{options.Summary, TKit::FileOpenFlag_AppendOrCreate};
    md.Format("### {} Onyx runtime tests{}{}\n\n", bad == 0 ? "✅" : "❌", options.Label.IsEmpty() ? "" : ": ",
              options.Label);
    md.Format("**{} passed, {} failed, {} crashed, {} timed out, {} skipped, {} known issues** in {:.1f} s\n\n",
              counts[Status_Pass], counts[Status_Fail], counts[Status_Crash], counts[Status_Timeout],
              counts[Status_Skip], counts[Status_Known], seconds);
    if (bad + counts[Status_Skip] + counts[Status_Known] != 0)
    {
        md.Write("| Status | Test | Time | Details |\n|---|---|---|---|\n");
        for (const Result &r : results)
        {
            if (r.State == Status_Pass)
                continue;
            md.Format("| {} | `{}` | {:.2f} s | ", upperStatus(r.State), r.Test->Id, r.Seconds);
            if (r.State == Status_Skip)
                md.WriteMarkdownCell(r.SkipReason);
            else if (r.State == Status_Known)
                md.WriteMarkdownCell(StringView{r.Test->Options.KnownIssue});
            else
                details(r, [&md](const StringView line) {
                    md.WriteMarkdownCell(line);
                    md.Write("<br>");
                });
            md.Write(" |\n");
        }
        md.Write("\n");
    }
    md.Format("<details><summary>All {} tests</summary>\n\n| Status | Test | Time |\n|---|---|---|\n",
              results.GetSize());
    for (const Result &r : results)
        md.Format("| {} | `{}` | {:.2f} s |\n", upperStatus(r.State), r.Test->Id, r.Seconds);
    md.Write("\n</details>\n\n");
}

// GitHub shows these as annotations on the run and on the pull request. they go to stdout as they are, not as logs
static void writeAnnotations(const Array<Result> &results)
{
    const char *actions = std::getenv("GITHUB_ACTIONS");
    if (!actions || std::strcmp(actions, "true") != 0)
        return;
    for (const Result &r : results)
    {
        if (!isBad(r.State))
            continue;
        if (r.Failures.IsEmpty())
        {
            TKit::PrintLine("::error file={},line={},title={} {}::{}", RelativePath(r.Test->File), r.Test->Line,
                            r.Test->Id, StatusNames[r.State], r.ExitInfo.IsEmpty() ? StatusNames[r.State] : r.ExitInfo);
            continue;
        }
        for (const Failure &f : r.Failures)
        {
            // "file:line". a path may hold colons of its own: the last one counts
            usize colon = String::npos;
            for (usize at = f.Where.Find(':'); at != String::npos; at = f.Where.Find(':', at + 1))
                colon = at;
            const String file = colon == String::npos ? f.Where : f.Where.SubString(0, colon);
            const String line = colon == String::npos ? String{"1"} : f.Where.SubString(colon + 1);
            String message{};
            describe(f, [&message](const StringView part) {
                if (!message.IsEmpty())
                    message += "%0A";
                message += String{part};
            });
            TKit::PrintLine("::error file={},line={},title={}::{}", file, line, r.Test->Id, message);
        }
    }
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------------------------------------------------
//  The run
// ---------------------------------------------------------------------------------------------------------------------

int RunSupervisor(const Options &options)
{
    const Array<const TestInfo *> tests = selectTests(options);
    if (options.List)
    {
        for (const TestInfo *test : tests)
        {
            if (options.Output == Output_Events)
                Emit("test", test->Id, test->Suite, test->Options.Description,
                     test->Options.KnownIssue ? test->Options.KnownIssue : "", RelativePath(test->File),
                     String::Format("{}", test->Line));
            else
                TKit::PrintLine("{:<56} {}", test->Id, test->Options.Description);
        }
        return 0;
    }
    if (tests.IsEmpty())
    {
        TKIT_LOG_ERROR("[ONYX-TESTS] No test matches the given patterns (see --list)");
        return 2;
    }

    const u32 total = u32(tests.GetSize());
    const u32 jobs = Math::Min(Math::Min(options.Jobs, total), MaxJobs);
    if (options.Output == Output_Events)
        Emit("plan", String::Format("{}", total));
    else
        TKIT_LOG_INFO("[ONYX-TESTS] {} tests, {} {} at a time{}{}", total, jobs, options.Isolate ? "tests" : "workers",
                      options.Label.IsEmpty() ? "" : ", ", options.Label);

    Run run{options};
    run.Tests = tests;
    run.Results.Resize(total);
    run.Shared.Resize(total);
    run.Retried.Resize(total);
    run.DuringTeardown.Resize(total);
    run.Stages.Resize(total);
    for (u32 i = 0; i < total; ++i)
    {
        run.Results[i].Test = tests[i];
        run.Retried[i] = false;
        run.DuringTeardown[i] = false;
        run.Stages[i] = Stage_Queued;
    }
    // enough batches for every job to have a few: the tests of a worker run one after another
    run.BatchSize = Math::Clamp((total + 2 * jobs - 1) / (2 * jobs), 1u, MaxBatchSize);

    TKit::Clock clock{};
    const auto work = [&run] {
        // job threads collect what their tests report: each one needs an allocator. the thread this is called from
        // already has one, and then this does nothing
        PushThreadAllocator(SupervisorMaxAllocation);
        Batch batch{};
        while (nextBatch(run, batch))
            runBatch(run, batch);
    };
    // one worker at a time runs here. Toolkit's thread pool takes two workers at least
    if (jobs == 1)
        work();
    else
    {
        // every pool thread takes batches until none is left. each one mostly waits on its worker process
        TKit::ThreadPool pool{jobs};
        // tasks cannot move, so they live in an array that never relocates
        TKit::StaticArray<TKit::Task<Storage>, MaxJobs> tasks{};
        tasks.Resize(jobs);
        for (TKit::Task<Storage> &task : tasks)
        {
            task = work;
            pool.SubmitTask(&task);
        }
        for (TKit::Task<Storage> &task : tasks)
            task.WaitUntilFinished();
    }
    const Array<Result> &results = run.Results;
    const f64 seconds = clock.GetElapsed().AsSeconds<f64>();

    u32 counts[Status_Count]{};
    for (const Result &r : results)
        ++counts[r.State];
    const u32 bad = counts[Status_Fail] + counts[Status_Crash] + counts[Status_Timeout];

    if (options.Output == Output_Events)
        Emit("summary", String::Format("{}", counts[Status_Pass]), String::Format("{}", counts[Status_Fail]),
             String::Format("{}", counts[Status_Skip]), String::Format("{}", counts[Status_Crash]),
             String::Format("{}", counts[Status_Timeout]), String::Format("{}", counts[Status_Known]),
             String::Format("{}", u64(seconds * 1000.0)));
    else
    {
        const String line = String::Format("[ONYX-TESTS] {} tests: {} passed, {} failed, {} crashed, {} timed out, {} "
                                           "skipped, {} known issues ({:.1f} s)",
                                           total, counts[Status_Pass], counts[Status_Fail], counts[Status_Crash],
                                           counts[Status_Timeout], counts[Status_Skip], counts[Status_Known], seconds);
        logLine(bad == 0 ? Status_Pass : Status_Fail, line);
        for (const Result &r : results)
            if (isBad(r.State))
                logLine(r.State, String::Format("[ONYX-TESTS]   {:<7} {}", upperStatus(r.State), r.Test->Id));
        for (u32 i = 0; i < total; ++i)
            if (run.Retried[i] && !isBad(results[i].State))
                logLine(Status_Skip, String::Format("[ONYX-TESTS]   {} only passed alone: a test before it leaves "
                                                    "something behind (see its note)",
                                                    results[i].Test->Id));
    }

    if (!options.JUnit.IsEmpty())
        writeJUnit(options, results, seconds);
    if (!options.Summary.IsEmpty())
        writeSummary(options, results, seconds);
    writeAnnotations(results);
    return bad == 0 ? 0 : 1;
}
} // namespace OnyxTest
