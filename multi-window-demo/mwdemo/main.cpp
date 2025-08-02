#include "mwdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "utils/argparse.hpp"

void RunApp(const Onyx::Demo::Scene p_Scene) noexcept
{
    Onyx::MultiWindowApplication app;

    app.SetUserLayer<Onyx::Demo::MWExampleLayer>(&app, p_Scene);
    app.OpenWindow({.Name = "Main window"});
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
