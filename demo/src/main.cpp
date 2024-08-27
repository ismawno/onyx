#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "kit/core/literals.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;

int main()
{
    KIT::StackAllocator stackAllocator(1_kb);
    KIT::ThreadPool<KIT::SpinMutex> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application2D app1;
    ONYX::Application2D app2;

    const auto task1 = threadPool.CreateAndSubmit([&app1](const KIT::usize) { app1.Run(); });
    const auto task2 = threadPool.CreateAndSubmit([&app2](const KIT::usize) { app2.Run(); });

    while (!task1->Finished() || !task2->Finished())
        ONYX::Input::PollEvents();

    ONYX::Core::Terminate();
}