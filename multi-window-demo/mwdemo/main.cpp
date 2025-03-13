#include "mwdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/utils/literals.hpp"

void RunApp() noexcept
{
    Onyx::MultiWindowApplication app;
    app.SetUserLayer<Onyx::Demo::MWExampleLayer>(&app);
    app.OpenWindow({.Name = "Main window"});
    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool{7};
    Onyx::Core::Initialize(&threadPool);
    RunApp();
    Onyx::Core::Terminate();
}