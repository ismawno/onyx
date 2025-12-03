#include "perf/argparse.hpp"
#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/macros.hpp"

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

void RunApp(const Onyx::Demo::ParseResult &p_Args)
{
    Onyx::Window::Specs spc;
    spc.Name = "Performance lattice";
    spc.PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    Onyx::Application app{spc};
    if (p_Args.Dim == Onyx::Dimension::D2)
        app.SetUserLayer<Onyx::Demo::Layer<Onyx::Dimension::D2>>(p_Args.Lattices2);
    else
        app.SetUserLayer<Onyx::Demo::Layer<Onyx::Dimension::D3>>(p_Args.Lattices3);
    if (!p_Args.HasRuntime)
        app.Run();
    else
    {
        TKit::Clock frameClock{};
        TKit::Clock runTimeClock{};
        while (runTimeClock.GetElapsed().AsSeconds() < p_Args.RunTime && app.NextFrame(frameClock))
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
