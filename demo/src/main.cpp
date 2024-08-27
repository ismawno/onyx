#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/literals.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;

int main()
{
    KIT::StackAllocator stackAllocator(1_kb);
    // KIT::ThreadPool<KIT::SpinMutex> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, nullptr);

    ONYX::Application2D app;
    app.Run();

    ONYX::Core::Terminate();
}