#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/utils/literals.hpp"

void RunApp() noexcept
{
    Onyx::Window::Specs spc;
    spc.Name = "Single window demo app";

    Onyx::Application app{spc};
    app.SetUserLayer<Onyx::Demo::SWExampleLayer>(&app);
    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool{7};
    Onyx::Core::Initialize(&threadPool);
    RunApp();
    Onyx::Core::Terminate();
}