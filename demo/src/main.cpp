#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "onyx/camera/orthographic.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;

int main()
{
    KIT::StackAllocator stackAllocator(1_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application app;
    app.OpenWindow<ONYX::Orthographic2D>();

    app.Start();
    while (app.NextFrame())
        ;
    app.Shutdown();

    ONYX::Core::Terminate();
}