#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "utils/argparse.hpp"

void RunApp(const Onyx::Demo::Scene p_Scene) noexcept
{
    Onyx::Window::Specs spc;
    spc.Name = "Single window demo app";

    Onyx::Application app{spc};
    app.SetUserLayer<Onyx::Demo::SWExampleLayer>(&app, p_Scene);
    app.Run();
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const Onyx::Demo::Scene scene = Onyx::Demo::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_THREADS - 1};
    Onyx::Core::Initialize(&threadPool);
    RunApp(scene);
    Onyx::Core::Terminate();
}
