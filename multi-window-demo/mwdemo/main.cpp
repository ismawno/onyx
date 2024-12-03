#include "mwdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include "tkit/memory/stack_allocator.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

using namespace TKit::Literals;

template <Onyx::WindowThreading Threading> void RunApp() noexcept
{
    Onyx::MultiWindowApplication<Threading> app;
    app.Layers.template Push<Onyx::MWExampleLayer>(&app);

    Onyx::Window::Specs spc{};
    spc.Name = "Main window";
    app.OpenWindow(spc);
    app.Run();
}

int main()
{
    TKit::ThreadPool<std::mutex> threadPool{7};
    TKit::StackAllocator allocator{10_kb};
    Onyx::Core::Initialize(&allocator, &threadPool);
    RunApp<Onyx::Serial>();
    Onyx::Core::Terminate();
}