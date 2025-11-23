#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "utils/argparse.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/macros.hpp"

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

void RunApp(const Onyx::Demo::Scene p_Scene)
{
    Onyx::Window::Specs spc;
    spc.Name = "Single window demo app";

    Onyx::Application app{spc};
    app.InitializeImGui();
    app.SetUserLayer<Onyx::Demo::SWExampleLayer>(&app, p_Scene);
    app.Run();
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const Onyx::Demo::Scene scene = Onyx::Demo::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};
    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});
    RunApp(scene);
    Onyx::Core::Terminate();
}
