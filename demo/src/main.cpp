#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/draw/primitives/rectangle.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

using namespace KIT::Literals;
using namespace KIT::Aliases;

class ExampleLayer final : public ONYX::Layer
{
  public:
    using ONYX::Layer::Layer;

    void OnRender(const usize p_WindowIndex) noexcept override
    {
        ONYX::Rectangle2D rect(ONYX::Color::RED);
        auto window = GetApplication()->GetWindow(p_WindowIndex);
        window->Draw(rect);
    }
};

int main()
{
    KIT::StackAllocator stackAllocator(10_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application<ONYX::MultiWindowFlow::CONCURRENT> app;
    app.Layers.Push<ExampleLayer>("Example Layer");

    auto window1 = app.OpenWindow<ONYX::Orthographic2D>();
    app.OpenWindow<ONYX::Orthographic2D>();

    auto cam = window1->GetCamera<ONYX::Orthographic2D>();
    cam->FlipY();
    cam->SetSize(5.f);

    app.Run();

    ONYX::Core::Terminate();
}