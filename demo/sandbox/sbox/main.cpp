#include "sbox/layer.hpp"
#include "sbox/argparse.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/macros.hpp"

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

void RunApp(const Onyx::Demo::ArgResult p_Args)
{

    Onyx::Demo::QuitResult quitResult;
    quitResult.Type = p_Args.AppType;
    for (;;)
    {
        if (quitResult.Type == Onyx::Demo::ApplicationType::SingleWindow)
        {
            Onyx::Window::Specs spc;
            spc.Name = "Single window demo app";

            Onyx::SingleWindowApp app{spc};
            app.InitializeImGui();
            const Onyx::Demo::SandboxLayer *layer = app.SetUserLayer<Onyx::Demo::SandboxLayer>(&app, p_Args.Dim);
            app.Run();
            quitResult = layer->QuitResult;
        }
        else
        {
            Onyx::MultiWindowApp app{};
            const Onyx::Demo::SandboxLayer *layer = app.SetUserLayer<Onyx::Demo::SandboxLayer>(&app, p_Args.Dim);
            app.OpenWindow();
            app.InitializeImGui();
            app.Run();
            quitResult = layer->QuitResult;
        }
        if (!quitResult.Reload)
            break;
    }
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const Onyx::Demo::ArgResult result = Onyx::Demo::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};
    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});
    RunApp(result);
    Onyx::Core::Terminate();
}
