#include "mwdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

void RunApp() noexcept
{
    Onyx::MultiWindowApplication app;
    app.SetUserLayer<Onyx::Demo::MWExampleLayer>(&app);

    Onyx::Window::Specs spc{};
    spc.Name = "Main window";
    app.OpenWindow(spc);
    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool{7};
    Onyx::Core::Initialize(&threadPool);
    RunApp();
    Onyx::Core::Terminate();
}