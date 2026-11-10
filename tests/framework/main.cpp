// onyx-tests: the Onyx runtime tests, run in worker processes a crash can take down without taking the run with it.
//
// Two roles live in this executable:
//   - supervisor (the default, supervisor.cpp): selects tests, hands them to worker processes, collects what they
//     report and writes the reports
//   - worker (--worker <ids>, worker.cpp): starts Onyx, runs its tests through Catch2, reports and exits
//
// Workers talk to the supervisor with the line protocol in protocol.hpp. The supervisor speaks the same protocol to
// tools when asked to (--format events): that is how the control center drives it.

#include "options.hpp"
#include <cstdlib>
#include <cstring>

namespace OnyxTest
{
// main's own: options hold strings, and there is no allocator to give them any during static initialization
static const Options *s_Options = nullptr;
const Options &GetOptions()
{
    return *s_Options;
}

static void printUsage()
{
    TKit::PrintLine(
        "Usage: onyx-tests [options] [pattern...]\n"
        "\n"
        "Runs the Onyx runtime tests, in worker processes of their own. Patterns select tests by id (suite.name) or "
        "by suite,\n"
        "and may use * and ?. Several patterns add up, and a pattern that starts with - excludes:\n"
        "  onyx-tests render 'overlay.*' '-*msaa*'\n"
        "\n"
        "  --list              List the selected tests and exit\n"
        "  --jobs <n>          Run n worker processes at a time (default 1)\n"
        "  --isolate           Give every test a process of its own, with an Onyx no other test has used. By default\n"
        "                      tests that pass share one, which is several times faster, and only a test that goes\n"
        "                      wrong after another is run again alone\n"
        "  --format <f>        human (default), or events: a line protocol for tools such as the control center\n"
        "  --junit <path>      Also write a JUnit XML report\n"
        "  --summary <path>    Also write a Markdown summary. On GitHub Actions it goes to $GITHUB_STEP_SUMMARY\n"
        "                      by default\n"
        "  --label <text>      Name this run in the reports, e.g. 'linux-clang debug'\n"
        "  --timeout-scale <f> Multiply every test timeout (slow machines, sanitizers)\n"
        "  --speed <s>         How fast overlay engine tests play the user: fast (default), normal or cinematic.\n"
        "                      Slower speeds move the mouse like a person would, so a run can be watched\n"
        "  --verbose           Print the output of every test, not only of those that fail\n"
        "  --worker <ids>      Run the given tests (separated by commas) in this process, without a supervisor\n"
        "                      (handy under a debugger)");
}

bool ParseOptions(const int argc, char **argv, Options &options)
{
    options.Self = argv[0];
    for (int i = 1; i < argc; ++i)
    {
        const String arg{argv[i]};
        const auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            std::exit(0);
        }
        else if (arg == "--list")
            options.List = true;
        else if (arg == "--verbose")
            options.Verbose = true;
        else if (arg == "--isolate")
            options.Isolate = true;
        else if (arg == "--jobs" || arg == "--worker" || arg == "--format" || arg == "--junit" || arg == "--summary" ||
                 arg == "--label" || arg == "--timeout-scale" || arg == "--speed")
        {
            const char *value = next();
            if (!value)
            {
                TKIT_LOG_ERROR("[ONYX-TESTS] {} needs a value", arg);
                return false;
            }
            const String v{value};
            if (arg == "--jobs")
            {
                const int jobs = std::atoi(value);
                options.Jobs = u32(jobs < 1 ? 1 : jobs);
            }
            else if (arg == "--worker")
                options.Worker = v;
            else if (arg == "--junit")
                options.JUnit = value;
            else if (arg == "--summary")
                options.Summary = value;
            else if (arg == "--label")
                options.Label = v;
            else if (arg == "--timeout-scale")
                options.TimeoutScale = f32(std::atof(value));
            else if (arg == "--speed")
            {
                u32 speed = 0;
                while (speed < Speed_Count && v != SpeedNames[speed])
                    ++speed;
                if (speed == Speed_Count)
                {
                    TKIT_LOG_ERROR("[ONYX-TESTS] Unknown speed '{}': fast, normal or cinematic", value);
                    return false;
                }
                options.PlaySpeed = Speed(speed);
            }
            else if (v == "events")
                options.Output = Output_Events;
            else if (v == "human")
                options.Output = Output_Human;
            else
            {
                TKIT_LOG_ERROR("[ONYX-TESTS] Unknown format '{}'", value);
                return false;
            }
        }
        else if (arg.GetSize() > 1 && arg[0] == '-' && arg[1] != '-')
            options.Exclude.Append(arg.SubString(1));
        else if (arg.StartsWith("--"))
        {
            TKIT_LOG_ERROR("[ONYX-TESTS] Unknown option '{}' (see --help)", arg);
            return false;
        }
        else
            // a comma separated list is accepted too, which keeps command lines short when a tool passes many ids
            ForEachPart(arg, ',', [&options](const StringView pattern) {
                if (!pattern.IsEmpty())
                    options.Include.Append(pattern);
            });
    }
    if (const char *summary = std::getenv("GITHUB_STEP_SUMMARY"); options.Summary.IsEmpty() && summary && *summary)
        options.Summary = summary;
    return true;
}
} // namespace OnyxTest

int main(const int argc, char **argv)
{
    OnyxTest::SilenceCrashDialogs();

    // before the first string: there is no allocator yet
    bool worker = false;
    for (int i = 1; i < argc; ++i)
        worker |= std::strcmp(argv[i], "--worker") == 0;
    OnyxTest::PushThreadAllocator(worker ? OnyxTest::WorkerMaxAllocation : OnyxTest::SupervisorMaxAllocation);

    OnyxTest::Options options{};
    if (!OnyxTest::ParseOptions(argc, argv, options))
        return 2;
    OnyxTest::s_Options = &options;
    if (!options.Worker.IsEmpty())
        return OnyxTest::RunWorker(options);
    return OnyxTest::RunSupervisor(options);
}
