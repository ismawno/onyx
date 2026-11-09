// The supervisor: selects tests, runs each one in a worker process of its own (--worker <id>), collects what the worker
// reports and writes the reports. A worker that crashes, panics on a validation error or hangs only takes its own test
// down. People read Toolkit's log lines; tools read protocol events (--format events, protocol.hpp).

#include "options.hpp"
#include "protocol.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/filesystem/file.hpp"
#include "tkit/multiprocessing/task.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/clock.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#    define popen _popen
#    define pclose _pclose
#else
#    include <sys/wait.h>
#endif

namespace OnyxTest
{
using String = TKit::DynamicString;
template <typename T> using Array = TKit::DynamicArray<T>;

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
    // the last lines the test printed, shown for failures
    Array<String> Output{};
};

static constexpr u32 MaxJobs = 64;

// a line as text, without the terminal's color codes (Toolkit colors its log levels) or other control characters.
// they have no glyph in the control center's font, and they are not valid XML 1.0, which the JUnit report is
static String plainText(const String &line)
{
    String out{};
    for (usz i = 0; i < line.GetSize(); ++i)
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

// a failure as people read it, one line each: where and what, what Catch2 expanded it to, the messages in force
static Array<String> describe(const Failure &f)
{
    Array<String> lines{};
    lines.Append(f.Expression.IsEmpty() ? f.Where : String::Format("{}: {}", f.Where, f.Expression));
    if (!f.Expansion.IsEmpty() && f.Expansion != f.Expression)
        for (const String &part : f.Expansion.Split('\n'))
            lines.Append(String::Format("    with expansion: {}", part));
    if (!f.Info.IsEmpty())
        for (const String &part : f.Info.Split('\n'))
            lines.Append(String::Format("    {}", part));
    return lines;
}

static String details(const Result &result)
{
    String text{};
    for (const Failure &f : result.Failures)
        for (const String &line : describe(f))
            text += line + "\n";
    if (!result.ExitInfo.IsEmpty())
        text += result.ExitInfo + "\n";
    for (const String &note : result.Notes)
        text += "note: " + note + "\n";
    return text;
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
        return matches(pattern.CString(), test.Id.CString()) || pattern == test.Suite;
    };
    Array<const TestInfo *> selected{};
    for (const TestInfo &test : GetTests())
    {
        bool included = options.Include.IsEmpty();
        for (const String &pattern : options.Include)
            included |= hits(pattern, test);
        for (const String &pattern : options.Exclude)
            included &= !hits(pattern, test);
        if (included)
            selected.Append(&test);
    }
    // suites in alphabetical order, tests in the order they are written in
    for (usz i = 1; i < selected.GetSize(); ++i)
        for (usz j = i; j > 0 && String{selected[j]->Suite} < String{selected[j - 1]->Suite}; --j)
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

static String workerCommand(const Options &options, const TestInfo &test)
{
    String command = String::Format("\"{}\" --worker {} --speed {} --timeout-scale {} 2>&1", options.Self, test.Id,
                                    options.Speed, options.TimeoutScale);
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

static void runWorker(const Options &options, Result &result)
{
    const TestInfo &test = *result.Test;
    TKit::Clock clock{};
    const String command = workerCommand(options, test);

    std::FILE *pipe = popen(command.CString(), "r");
    if (!pipe)
    {
        result.State = Status_Crash;
        result.ExitInfo = "Could not start the worker process";
        return;
    }

    bool ended = false;
    String line{};
    Array<String> fields{};
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe))
    {
        line += buffer;
        if (line.IsEmpty() || line.GetBack() != '\n')
            continue;
        while (!line.IsEmpty() && (line.GetBack() == '\n' || line.GetBack() == '\r'))
            line.Pop();

        if (!ParseEvent(line, fields))
        {
            if (!isCatchNoise(line))
                result.Output.Append(plainText(line));
            line.Clear();
            continue;
        }
        line.Clear();
        const String &kind = fields[0];
        if (kind == "fail" && fields.GetSize() >= 5)
            result.Failures.Append(Failure{fields[1], fields[2], fields[3], fields[4]});
        else if (kind == "skip" && fields.GetSize() >= 2)
            result.SkipReason = fields[1];
        else if (kind == "note" && fields.GetSize() >= 2)
            result.Notes.Append(fields[1]);
        else if (kind == "end" && fields.GetSize() >= 2)
        {
            ended = true;
            for (u32 i = 0; i < Status_Count; ++i)
                if (fields[1] == StatusNames[i])
                    result.State = Status(i);
        }
    }

    const int code = pclose(pipe);
    result.Seconds = clock.GetElapsed().AsSeconds<f64>();
    if (!ended)
    {
        result.State = Status_Crash;
#ifdef _WIN32
        result.ExitInfo = String::Format("Exit code {}", code);
#else
        if (WIFSIGNALED(code))
            result.ExitInfo = String::Format("Killed by signal {}", WTERMSIG(code));
        else if (WIFEXITED(code))
            result.ExitInfo = String::Format("Exit code {}", WEXITSTATUS(code));
#endif
        if (result.ExitInfo.IsEmpty())
            result.ExitInfo = "The worker ended without reporting";
    }
    // a worker that reports a pass and then fails to exit cleanly still lost something
    else if (result.State == Status_Pass && code != 0)
    {
        result.State = Status_Crash;
        result.ExitInfo = String::Format("Reported a pass but exited with status {}", code);
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
        if (!result.ExitInfo.IsEmpty())
            Emit("crash", test.Id, result.ExitInfo);
        if (isBad(result.State) || options.Verbose)
            for (const String &line : result.Output)
                Emit("output", test.Id, line);
        Emit("end", test.Id, StatusNames[result.State], String::Format("{}", u64(result.Seconds * 1000.0)));
        return;
    }

    const String count = String::Format("{}", total);
    String headline = String::Format("[ONYX-TESTS] [{:>{}}/{}] {:<7} {} ({:.0f} ms)", index + 1, count.GetSize(), total,
                                     upperStatus(result.State), test.Id, result.Seconds * 1000.0);
    if (result.State == Status_Skip && !result.SkipReason.IsEmpty())
        headline += ": " + result.SkipReason;
    logLine(result.State, headline);
    for (const Failure &f : result.Failures)
        for (const String &line : describe(f))
            logLine(result.State, "    " + line);
    if (!result.ExitInfo.IsEmpty())
        logLine(result.State, "    " + result.ExitInfo);
    for (const String &note : result.Notes)
        logLine(result.State, "    note: " + note);
    if ((isBad(result.State) || options.Verbose) && !result.Output.IsEmpty())
    {
        constexpr u32 maxLines = 80;
        const u32 size = Math::Min(maxLines, result.Output.GetSize());
        logLine(result.State, String::Format("    --- The last {} lines it printed ---", size));
        for (u32 i = 0; i < size; ++i)
            logLine(result.State, "    | " + result.Output[result.Output.GetSize() - i - 1]);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//  Reports: JUnit, Markdown summary, GitHub annotations
// ---------------------------------------------------------------------------------------------------------------------

static String xmlEscape(const String &text)
{
    String out{};
    for (const char c : text)
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            // control characters are not allowed in XML 1.0, and logs carry ANSI colors
            if (u8(c) >= 0x20 || c == '\n' || c == '\t')
                out += c;
        }
    return out;
}

static void writeFile(const String &path, const String &contents, const TKit::FileOpenFlags flags)
{
    const auto written = TKit::WriteFile(path, TKit::StringView{contents}, flags);
    TKIT_LOG_ERROR_IF(!written, "[ONYX-TESTS] Could not write '{}'", path);
}

static void writeJUnit(const Options &options, const Array<Result> &results, const f64 seconds)
{
    u32 counts[Status_Count]{};
    for (const Result &r : results)
        ++counts[r.State];

    String xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += String::Format(
        "<testsuites name=\"onyx-tests{}{}\" tests=\"{}\" failures=\"{}\" errors=\"{}\" skipped=\"{}\" "
        "time=\"{:.3f}\">\n",
        options.Label.IsEmpty() ? "" : " ", xmlEscape(options.Label), results.GetSize(), counts[Status_Fail],
        counts[Status_Crash] + counts[Status_Timeout], counts[Status_Skip] + counts[Status_Known], seconds);

    usz i = 0;
    while (i < results.GetSize())
    {
        const String suite{results[i].Test->Suite};
        usz end = i;
        u32 suiteCounts[Status_Count]{};
        f64 suiteSeconds = 0.0;
        while (end < results.GetSize() && suite == results[end].Test->Suite)
        {
            ++suiteCounts[results[end].State];
            suiteSeconds += results[end].Seconds;
            ++end;
        }
        xml += String::Format("  <testsuite name=\"{}\" tests=\"{}\" failures=\"{}\" errors=\"{}\" skipped=\"{}\" "
                              "time=\"{:.3f}\">\n",
                              suite, end - i, suiteCounts[Status_Fail],
                              suiteCounts[Status_Crash] + suiteCounts[Status_Timeout],
                              suiteCounts[Status_Skip] + suiteCounts[Status_Known], suiteSeconds);
        for (; i < end; ++i)
        {
            const Result &r = results[i];
            xml += String::Format("    <testcase classname=\"{}\" name=\"{}\" file=\"{}\" line=\"{}\" time=\"{:.3f}\">",
                                  r.Test->Suite, r.Test->Name, xmlEscape(RelativePath(r.Test->File)), r.Test->Line,
                                  r.Seconds);
            const String text = xmlEscape(details(r));
            if (r.State == Status_Fail)
            {
                const String first = r.Failures.IsEmpty() ? String{"failed"} : describe(r.Failures[0])[0];
                xml += String::Format("\n      <failure message=\"{}\">{}</failure>", xmlEscape(first), text);
            }
            else if (r.State == Status_Crash || r.State == Status_Timeout)
                xml += String::Format("\n      <error message=\"{}\">{}</error>", StatusNames[r.State], text);
            else if (r.State == Status_Skip)
                xml += String::Format("\n      <skipped message=\"{}\"/>", xmlEscape(r.SkipReason));
            else if (r.State == Status_Known)
                xml += String::Format("\n      <skipped message=\"known issue: {}\">{}</skipped>",
                                      xmlEscape(String{r.Test->Options.KnownIssue}), text);
            if (isBad(r.State) && !r.Output.IsEmpty())
            {
                String out{};
                for (const String &line : r.Output)
                    out += line + "\n";
                xml += String::Format("\n      <system-out>{}</system-out>", xmlEscape(out));
            }
            xml += r.State == Status_Pass ? "</testcase>\n" : "\n    </testcase>\n";
        }
        xml += "  </testsuite>\n";
    }
    xml += "</testsuites>\n";
    writeFile(options.JUnit, xml, TKit::FileOpenFlag_Overwrite);
}

static String markdownCell(const String &text)
{
    String out{};
    for (const char c : text)
    {
        if (c == '|')
            out += "\\|";
        else if (c == '\n')
            out += "<br>";
        else if (u8(c) >= 0x20)
            out += c;
    }
    return out;
}

static void writeSummary(const Options &options, const Array<Result> &results, const f64 seconds)
{
    u32 counts[Status_Count]{};
    for (const Result &r : results)
        ++counts[r.State];
    const u32 bad = counts[Status_Fail] + counts[Status_Crash] + counts[Status_Timeout];

    String md = String::Format("### {} Onyx runtime tests{}{}\n\n", bad == 0 ? "✅" : "❌",
                               options.Label.IsEmpty() ? "" : ": ", options.Label);
    md += String::Format(
        "**{} passed, {} failed, {} crashed, {} timed out, {} skipped, {} known issues** in {:.1f} s\n\n",
        counts[Status_Pass], counts[Status_Fail], counts[Status_Crash], counts[Status_Timeout], counts[Status_Skip],
        counts[Status_Known], seconds);
    if (bad + counts[Status_Skip] + counts[Status_Known] != 0)
    {
        md += "| Status | Test | Time | Details |\n|---|---|---|---|\n";
        for (const Result &r : results)
            if (r.State != Status_Pass)
            {
                const String cell = r.State == Status_Skip    ? r.SkipReason
                                    : r.State == Status_Known ? String{r.Test->Options.KnownIssue}
                                                              : details(r);
                md += String::Format("| {} | `{}` | {:.2f} s | {} |\n", upperStatus(r.State), r.Test->Id, r.Seconds,
                                     markdownCell(cell));
            }
        md += "\n";
    }
    md += String::Format("<details><summary>All {} tests</summary>\n\n| Status | Test | Time |\n|---|---|---|\n",
                         results.GetSize());
    for (const Result &r : results)
        md += String::Format("| {} | `{}` | {:.2f} s |\n", upperStatus(r.State), r.Test->Id, r.Seconds);
    md += "\n</details>\n\n";
    // appended: on GitHub several steps (one per configuration) share the summary file
    writeFile(options.Summary, md, TKit::FileOpenFlag_AppendOrCreate);
}

// GitHub shows these as annotations on the run and on the pull request. they go to stdout as they are, not as logs
static void writeAnnotations(const Array<Result> &results)
{
    const char *actions = std::getenv("GITHUB_ACTIONS");
    if (!actions || String{actions} != "true")
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
            const Array<String> parts = f.Where.Split(':');
            const String line = parts.GetSize() > 1 ? parts[parts.GetSize() - 1] : String{"1"};
            const String file = f.Where.SubString(0, f.Where.GetSize() - line.GetSize() - (parts.GetSize() > 1));
            String message{};
            for (const String &l : describe(f))
                message += (message.IsEmpty() ? "" : "%0A") + l;
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
        TKIT_LOG_INFO("[ONYX-TESTS] {} tests, {} at a time{}{}", total, jobs, options.Label.IsEmpty() ? "" : ", ",
                      options.Label);

    Array<Result> results{};
    results.Resize(total);
    for (u32 i = 0; i < total; ++i)
        results[i].Test = tests[i];

    TKit::Clock clock{};
    std::atomic<u32> next{0};
    std::atomic<u32> completed{0};
    const auto work = [&] {
        for (u32 i = next++; i < total; i = next++)
        {
            if (options.Output == Output_Events)
                Emit("begin", tests[i]->Id);
            runWorker(options, results[i]);
            reportResult(options, results[i], completed++, total);
        }
    };
    // one test at a time runs here. Toolkit's thread pool takes two workers at least
    if (jobs == 1)
        work();
    else
    {
        // every pool thread pulls tests until none is left. each one mostly waits on its worker process
        TKit::ThreadPool pool{jobs};
        // tasks cannot move, so they live in an array that never relocates
        TKit::StaticArray<TKit::DynamicTask<>, MaxJobs> tasks{};
        tasks.Resize(jobs);
        for (TKit::DynamicTask<> &task : tasks)
        {
            task = work;
            pool.SubmitTask(&task);
        }
        for (TKit::DynamicTask<> &task : tasks)
            task.WaitUntilFinished();
    }
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
    }

    if (!options.JUnit.IsEmpty())
        writeJUnit(options, results, seconds);
    if (!options.Summary.IsEmpty())
        writeSummary(options, results, seconds);
    writeAnnotations(results);
    return bad == 0 ? 0 : 1;
}
} // namespace OnyxTest
