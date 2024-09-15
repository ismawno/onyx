#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/draw/primitives/rectangle.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;

int main()
{
    KIT::StackAllocator stackAllocator(10_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application app;
    auto window = app.OpenWindow<ONYX::Orthographic2D>();
    auto cam = window->GetCamera<ONYX::Orthographic2D>();
    cam->FlipY();

    ONYX::Rectangle2D rect(ONYX::Color::RED);

    app.Start();
    while (app.NextFrame())
    {
        app.Draw(rect);
        cam->Transform.Position += glm::vec2(0.001f);
    }
    app.Shutdown();

    ONYX::Core::Terminate();
}