// The worker: one process that runs the tests it is given, one after another. It brings Onyx to the state each test
// asks for, has Catch2 run the test cases and reports through the protocol (protocol.hpp) as it goes, so that a crash
// later on loses nothing already found.
//
// Tests that pass share Onyx: starting it (a device, the pipelines, the default font) takes far longer than most tests
// do, so it is initialized once for all of them and terminated after the last one. Between two tests the worker closes
// the windows the first one left open. A test that fails, skips, crashes or times out ends the process: what it left
// behind is not something another test should start from. The supervisor then runs the tests that did not get their
// turn in another worker (supervisor.cpp).
//
// Catch2 is built without exceptions, like Onyx. A failed REQUIRE, a SKIP or a FAIL then cannot unwind out of the
// test: Catch2 reports it and calls std::terminate. The terminate handler below turns that into the end of the test.

#include "options.hpp"
#include "protocol.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/resources.hpp"
#include "tkit/profiling/clock.hpp"
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/reporters/catch_reporter_streaming_base.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>

namespace OnyxTest
{
// the watchdog ends a test that hangs (an acquire that never returns, a loop waiting for an event that never comes). it
// reports the timeout itself, because nothing else will. it is armed for one test at a time
class Watchdog
{
  public:
    Watchdog()
        : m_Thread{[this] {
              // for the event it emits if a test times out
              PushThreadAllocator(WorkerMaxAllocation);
              std::unique_lock lock{m_Mutex};
              while (!m_Stopped)
              {
                  if (!m_Test)
                  {
                      m_Changed.wait(lock);
                      continue;
                  }
                  if (std::chrono::steady_clock::now() < m_Deadline)
                  {
                      m_Changed.wait_until(lock, m_Deadline);
                      continue;
                  }
                  Emit("end", m_Test, StatusNames[Status_Timeout],
                       String::Format("{}", u64(m_Clock.GetElapsed().AsMilliseconds<f64>())));
                  std::fflush(nullptr);
                  std::_Exit(124);
              }
          }}
    {
    }
    ~Watchdog()
    {
        {
            const std::lock_guard lock{m_Mutex};
            m_Stopped = true;
        }
        m_Changed.notify_all();
        if (m_Thread.joinable() && m_Thread.get_id() != std::this_thread::get_id())
            m_Thread.join();
    }

    void Arm(const char *test, const f64 seconds)
    {
        {
            const std::lock_guard lock{m_Mutex};
            m_Test = test;
            m_Clock.Restart();
            m_Deadline =
                std::chrono::steady_clock::now() +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<f64>(seconds));
        }
        m_Changed.notify_all();
    }
    void Disarm()
    {
        {
            const std::lock_guard lock{m_Mutex};
            m_Test = nullptr;
        }
        m_Changed.notify_all();
    }

  private:
    std::mutex m_Mutex{};
    std::condition_variable m_Changed{};
    std::chrono::steady_clock::time_point m_Deadline{};
    TKit::Clock m_Clock{};
    const char *m_Test = nullptr;
    bool m_Stopped = false;
    std::thread m_Thread;
};

enum OnyxState : u8
{
    Onyx_Down,
    // Onyx::Initialize with default specs
    Onyx_Bare,
    // and the default resources of both dimensions
    Onyx_Full,
};

static const Options *s_Options = nullptr;
static Watchdog *s_Watchdog = nullptr;
static OnyxState s_Onyx = Onyx_Down;
// the test in flight, and how many are still to start after it
static const TestInfo *s_Test = nullptr;
static u32 s_Remaining = 0;
static std::atomic<u32> s_Failures{0};
static std::atomic<bool> s_Skipped{false};
// Catch2 caught a signal: the test in flight crashed, and Catch2 raises the signal again once it has reported
static std::atomic<bool> s_Fatal{false};
static TKit::Clock s_Clock{};

static String elapsedMilliseconds()
{
    return String::Format("{}", u64(s_Clock.GetElapsed().AsMilliseconds<f64>()));
}

// ends the test in flight, and the process with it
[[noreturn]] static void finish(const Status status)
{
    if (s_Watchdog)
        s_Watchdog->Disarm();
    // a test that ends on a failed REQUIRE gets here without going through endTest
    RemoveTempFiles();
    Emit("end", s_Test->Id, StatusNames[status], elapsedMilliseconds());
    std::fflush(nullptr);
    // no global destructors: a failed test may have left Onyx half alive
    std::_Exit(status == Status_Fail ? 1 : 0);
}

static void onTerminate()
{
    // REQUIRE, FAIL and SKIP end the test this way. anything else that terminates is a crash, and stays one
    if (s_Test && s_Skipped)
        finish(Status_Skip);
    if (s_Test && s_Failures != 0)
        finish(Status_Fail);
    std::abort();
}

static void terminateOnyx()
{
    if (s_Onyx != Onyx_Down)
        Onyx::Terminate();
    s_Onyx = Onyx_Down;
}

static void beginTest(const TestInfo *test)
{
    s_Test = test;
    s_Failures = 0;
    s_Skipped = false;
    s_Clock.Restart();
    s_Watchdog->Arm(test->Id, f64(test->Options.Timeout * s_Options->TimeoutScale));
    // before Onyx starts: whatever goes wrong from here on belongs to this test
    Emit("begin", test->Id);

    switch (test->Options.Init)
    {
    case Init_Manual:
        // the test initializes Onyx itself
        terminateOnyx();
        break;
    case Init_Bare:
        // without the default resources: a test may be about creating them
        if (s_Onyx == Onyx_Full)
            terminateOnyx();
        if (s_Onyx == Onyx_Down)
        {
            Onyx::Initialize();
            s_Onyx = Onyx_Bare;
        }
        break;
    case Init_Full:
        if (s_Onyx == Onyx_Down)
        {
            Onyx::Initialize();
            s_Onyx = Onyx_Bare;
        }
        if (s_Onyx == Onyx_Bare)
        {
            Onyx::Resources_CreateDefault();
            s_Onyx = Onyx_Full;
        }
        break;
    }
}

static void endTest()
{
    // a crash stays a crash: Catch2 ends the test case on its way out of a signal, and then lets the signal through
    if (s_Fatal)
        return;
    // a failed test may have stopped half way through a frame. nothing may start from what it left, and tearing down
    // after it would only add a confusing crash to a failure that is already reported
    if (s_Skipped)
        finish(Status_Skip);
    if (s_Failures != 0)
        finish(Status_Fail);

    RemoveTempFiles();
    if (s_Test->Options.Init == Init_Manual)
    {
        // it terminated Onyx itself
        s_Onyx = Onyx_Down;
        ReleaseTestObjects(false);
    }
    else if (s_Remaining == 0)
    {
        // the last one: Terminate must cope with what the test left, open windows included. it runs before the test
        // is reported, so a teardown that crashes is a crash of the test. the supervisor is told it comes, as any of
        // the tests that shared Onyx may be the one it cannot cope with
        Emit("teardown");
        ReleaseTestObjects(false);
        terminateOnyx();
    }
    else
        // the next test shares Onyx: it starts without what this one left
        ReleaseTestObjects(s_Onyx != Onyx_Down);

    s_Watchdog->Disarm();
    Emit("end", s_Test->Id, StatusNames[Status_Pass], elapsedMilliseconds());
    s_Test = nullptr;
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

    void testCaseStarting(const Catch::TestCaseInfo &info) override
    {
        StreamingReporterBase::testCaseStarting(info);
        --s_Remaining;
        beginTest(FindTest(info.name.c_str()));
    }
    void testCaseEnded(const Catch::TestCaseStats &stats) override
    {
        StreamingReporterBase::testCaseEnded(stats);
        endTest();
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
        const String where = String::Format("{}:{}", RelativePath(source.file), source.line);
        const std::string expression = result.hasExpression() ? result.getExpressionInMacro() : std::string{};
        const std::string expansion = result.hasExpandedExpression() ? result.getExpandedExpression() : std::string{};

        // what FAIL says, and the INFO, CAPTURE and CHECK_MSG messages in force
        String info{};
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
        s_Fatal = true;
        Emit("note", String::Format("Fatal error: {}", std::string{error}));
    }
};
CATCH_REGISTER_REPORTER("onyx-worker", WorkerReporter)

int RunWorker(const Options &options)
{
    // the tests to run, as Catch2 takes them: names separated by commas
    u32 count = 0;
    bool known = true;
    ForEachPart(options.Worker, ',', [&](const StringView id) {
        const String name{id};
        ++count;
        if (FindTest(name.CString()))
            return;
        TKIT_LOG_ERROR("[ONYX-TESTS] No test named '{}' (see --list)", name);
        known = false;
    });
    if (!known)
        return 2;

        // the supervisor reads this through a pipe: nothing may sit in a buffer when the process dies
#ifdef _WIN32
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#else
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif

    s_Options = &options;
    s_Remaining = count;
    Watchdog watchdog{};
    s_Watchdog = &watchdog;
    std::set_terminate(onTerminate);

    Catch::Session session{};
    const char *args[] = {"onyx-tests", "--reporter", "onyx-worker", options.Worker.CString()};
    if (session.applyCommandLine(4, args) != 0)
    {
        TKIT_LOG_ERROR("[ONYX-TESTS] Catch2 did not accept '{}' as a test spec", options.Worker);
        return 2;
    }
    session.run();

    // every test passed and was reported, and the last one took Onyx down with it. unless Catch2 ran fewer tests than
    // it was given (a name listed twice): Onyx still goes down before the worker says it is done
    ReleaseTestObjects(false);
    terminateOnyx();
    Emit("done");
    std::fflush(nullptr);
    std::_Exit(0);
}
} // namespace OnyxTest
