#include "mwdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

template <Onyx::WindowThreading Threading> void RunApp() noexcept
{
    Onyx::MultiWindowApplication<Threading> app;
    app.Layers.template Push<Onyx::Demo::MWExampleLayer>(&app);

    Onyx::Window::Specs spc{};
    spc.Name = "Main window";
    app.OpenWindow(spc);
    app.Run();
}

int main()
{
    TKit::ThreadPool<std::mutex> threadPool{7};
    Onyx::Core::Initialize(&threadPool);
    RunApp<Onyx::Serial>();
    Onyx::Core::Terminate();
}