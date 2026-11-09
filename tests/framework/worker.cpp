// The worker: one test in this process. It initializes Onyx the way the test asks for, has Catch2 run the test case and
// reports through the protocol (protocol.hpp) as it goes, so that a crash later on loses nothing already found.
//
// Catch2 is built without exceptions, like Onyx. A failed REQUIRE, a SKIP or a FAIL then cannot unwind out of the
// test: Catch2 reports it and calls std::terminate. The terminate handler below turns that into the end of the test.

#include "options.hpp"
#include "protocol.hpp"
#include "onyx/core.hpp"
#include "onyx/resources.hpp"
#include "tkit/profiling/clock.hpp"
#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/reporters/catch_reporter_streaming_base.hpp>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>

namespace OnyxTest
{
static std::atomic<u32> s_Failures{0};
static std::atomic<bool> s_Skipped{false};
static TKit::Clock s_Clock{};

static TKit::DynamicString elapsedMilliseconds()
{
    return TKit::DynamicString::Format("{}", u64(s_Clock.GetElapsed().AsMilliseconds<f64>()));
}

// reports every assertion that did not pass, and skips and warnings, as protocol events
class WorkerReporter final : public Catch::StreamingReporterBase
{
  public:
    using StreamingReporterBase::StreamingReporterBase;

    static std::string getDescription()
    {
        return "the onyx-tests worker protocol (framework/protocol.hpp)";
    }

    void assertionEnded(const Catch::AssertionStats &stats) override
    {
        const Catch::AssertionResult &result = stats.assertionResult;
        const std::string message{result.getMessage()};
        if (result.getResultType() == Catch::ResultWas::ExplicitSkip)
        {
            s_Skipped = true;
            Emit("skip", message);
            return;
        }
        if (result.getResultType() == Catch::ResultWas::Warning)
        {
            Emit("note", message);
            return;
        }
        if (result.isOk())
            return;

        ++s_Failures;
        const Catch::SourceLineInfo source = result.getSourceInfo();
        const TKit::DynamicString where = TKit::DynamicString::Format("{}:{}", RelativePath(source.file), source.line);
        const std::string expression = result.hasExpression() ? result.getExpressionInMacro() : std::string{};
        const std::string expansion = result.hasExpandedExpression() ? result.getExpandedExpression() : std::string{};

        // what FAIL says, and the INFO, CAPTURE and CHECK_MSG messages in force
        TKit::DynamicString info{};
        const auto add = [&info](const std::string &line) {
            if (line.empty())
                return;
            if (!info.IsEmpty())
                info += '\n';
            info += line.c_str();
        };
        if (!result.hasExpression())
            add(message);
        for (const Catch::MessageInfo &m : stats.infoMessages)
            // FAIL's own message is among them too
            if (result.hasExpression() || m.message != message)
                add(m.message);
        Emit("fail", where, expression, expansion, info);
    }

    void fatalErrorEncountered(const Catch::StringRef error) override
    {
        Emit("note", TKit::DynamicString::Format("Fatal error: {}", std::string{error}));
    }
};
CATCH_REGISTER_REPORTER("onyx-worker", WorkerReporter)

// the watchdog ends a test that hangs (an acquire that never returns, a loop waiting for an event that never comes). it
// reports the timeout itself, because nothing else will
class Watchdog
{
  public:
    explicit Watchdog(const f64 seconds)
        : m_Thread{[this, seconds] {
              std::unique_lock lock{m_Mutex};
              if (m_Done.wait_for(lock, std::chrono::duration<f64>(seconds), [this] { return m_Finished; }))
                  return;
              Emit("end", StatusNames[Status_Timeout], elapsedMilliseconds());
              std::fflush(nullptr);
              std::_Exit(124);
          }}
    {
    }
    ~Watchdog()
    {
        Stop();
    }
    void Stop()
    {
        {
            const std::lock_guard lock{m_Mutex};
            m_Finished = true;
        }
        m_Done.notify_all();
        if (m_Thread.joinable() && m_Thread.get_id() != std::this_thread::get_id())
            m_Thread.join();
    }

  private:
    std::mutex m_Mutex{};
    std::condition_variable m_Done{};
    bool m_Finished = false;
    std::thread m_Thread;
};
static Watchdog *s_Watchdog = nullptr;

[[noreturn]] static void finish(const Status status)
{
    if (s_Watchdog)
        s_Watchdog->Stop();
    Emit("end", StatusNames[status], elapsedMilliseconds());
    std::fflush(nullptr);
    // no global destructors: a failed test may have left Onyx half alive
    std::_Exit(status == Status_Fail ? 1 : 0);
}

static void onTerminate()
{
    // REQUIRE, FAIL and SKIP end the test this way. anything else that terminates is a crash, and stays one
    if (s_Skipped)
        finish(Status_Skip);
    if (s_Failures != 0)
        finish(Status_Fail);
    std::abort();
}

int RunWorker(const Options &options)
{
    const TestInfo *test = FindTest(options.Worker.CString());
    if (!test)
    {
        TKIT_LOG_ERROR("[ONYX-TESTS] No test named '{}' (see --list)", options.Worker);
        return 2;
    }

    // the supervisor reads this through a pipe: nothing may sit in a buffer when the process dies
#ifdef _WIN32
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#else
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif

    s_Clock.Restart();
    Watchdog watchdog{f64(test->Options.Timeout * options.TimeoutScale)};
    s_Watchdog = &watchdog;
    std::set_terminate(onTerminate);

    if (test->Options.Init != Init_Manual)
    {
        Onyx::Initialize();
        if (test->Options.Init == Init_Full)
            Onyx::Resources_CreateDefault();
    }

    Catch::Session session{};
    const char *args[] = {"onyx-tests", "--reporter", "onyx-worker", test->Id.CString()};
    if (session.applyCommandLine(4, args) != 0)
    {
        Emit("note", "Catch2 did not accept the test's name as a test spec");
        finish(Status_Fail);
    }
    session.run();

    // while Onyx is still up: Toolkit's filesystem allocates from the tier allocator Onyx pushes
    RemoveTempFiles();
    // a failed test may have stopped half way through a frame. tearing down after it would only add a confusing crash
    // to a failure that is already reported, so only clean tests prove that Terminate copes with what they left
    if (test->Options.Init != Init_Manual && s_Failures == 0)
        Onyx::Terminate();

    finish(s_Failures != 0 ? Status_Fail : (s_Skipped ? Status_Skip : Status_Pass));
}
} // namespace OnyxTest
