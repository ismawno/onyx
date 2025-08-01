#include "perf/argparse.hpp"
#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"

void RunApp(const Onyx::Perf::ParseResult &p_Args) noexcept
{
    Onyx::Window::Specs spc;
    spc.Name = "Performance lattice";

    Onyx::Application app{spc};
    if (p_Args.Dim == Onyx::Dimension::D2)
        app.SetUserLayer<Onyx::Perf::Layer<Onyx::Dimension::D2>>(&app, p_Args.Lattices2);
    else
        app.SetUserLayer<Onyx::Perf::Layer<Onyx::Dimension::D3>>(&app, p_Args.Lattices3);
    if (!p_Args.HasRuntime)
        app.Run();
    else
    {
        app.Startup();
        TKit::Clock frameClock{};
        TKit::Clock runTimeClock{};
        while (runTimeClock.GetElapsed().AsSeconds() < p_Args.RunTime && app.NextFrame(frameClock))
            ;
        app.Shutdown();
    }
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP()
    const Onyx::Perf::ParseResult args = Onyx::Perf::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_THREADS - 1};
    Onyx::Core::Initialize(&threadPool);
    RunApp(args);
    Onyx::Core::Terminate();
}
