#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;

int main()
{
    KIT::StackAllocator stackAllocator(1_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application2D app;
    app.OpenWindow();
    // app.OpenWindow();

    app.Run();

    ONYX::Core::Terminate();
}