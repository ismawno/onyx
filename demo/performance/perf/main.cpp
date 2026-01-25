#include "perf/argparse.hpp"
#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/macros.hpp"

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

void RunApp(const Onyx::Demo::ParseResult &args)
{
    Onyx::Window::Specs spc;
    spc.Name = "Performance lattice";
    spc.PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    Onyx::Application app{spc};
    if (args.Dim == Onyx::Dimension::D2)
        app.SetUserLayer<Onyx::Demo::Layer<Onyx::Dimension::D2>>(args.Lattice2, args.Settings);
    else
        app.SetUserLayer<Onyx::Demo::Layer<Onyx::Dimension::D3>>(args.Lattice3, args.Settings);
    if (!args.HasRuntime)
        app.Run();
    else
    {
        TKit::Clock frameClock{};
        TKit::Clock runTimeClock{};
        while (runTimeClock.GetElapsed().AsSeconds() < args.RunTime && app.NextFrame(frameClock))
            ;
    }
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const Onyx::Demo::ParseResult args = Onyx::Demo::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};
    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});
    RunApp(args);
    Onyx::Core::Terminate();
}
