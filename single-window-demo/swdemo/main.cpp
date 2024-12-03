#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "tkit/memory/stack_allocator.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

using namespace TKit::Literals;

void RunApp() noexcept
{
    Onyx::Window::Specs spc;
    spc.Name = "Single window demo app";

    Onyx::Application app{spc};
    app.Layers.Push<Onyx::SWExampleLayer>(&app);
    app.Run();
}

int main()
{
    TKit::ThreadPool<std::mutex> threadPool{7};
    TKit::StackAllocator allocator{10_kb};
    Onyx::Core::Initialize(&allocator, &threadPool);
    RunApp();
    Onyx::Core::Terminate();
}