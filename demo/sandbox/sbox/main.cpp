#include "sbox/layer.hpp"
#include "sbox/argparse.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/profiling/macros.hpp"

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

void RunApp(const Onyx::Dimension p_Dim)
{
    Onyx::Window::Specs spc;
    spc.Name = "Onyx sandbox";

    Onyx::Application app{spc};
    app.SetUserLayer<Onyx::Demo::SandboxLayer>(p_Dim);
    app.Run();
}

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const Onyx::Dimension dim = Onyx::Demo::ParseArguments(argc, argv);

    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};
    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});
    RunApp(dim);
    Onyx::Core::Terminate();
}
