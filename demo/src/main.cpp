#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/draw/primitives/rectangle.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

#include <imgui.h>

using namespace KIT::Literals;
using namespace KIT::Aliases;
using namespace ONYX::Aliases;

class ExampleLayer final : public ONYX::Layer
{
  public:
    using ONYX::Layer::Layer;

    void OnRender(const usize p_WindowIndex) noexcept override
    {
        ONYX::Rectangle2D rect(ONYX::Color::RED);
        auto window = GetApplication()->GetWindow(p_WindowIndex);
        window->Draw(rect);
        rect.Color = ONYX::Color::GREEN;
        rect.Transform.Position += vec2(0.3f);
        window->Draw(rect);

        const float ts = GetApplication()->GetDeltaTime();
        window->GetCamera<ONYX::Orthographic2D>()->Transform.Rotation += ts;
    }

    void OnImGuiRender() noexcept override
    {
        const float ts = GetApplication()->GetDeltaTime();
        ImGui::Begin("Example Layer");
        ImGui::Text("Hello, World!");
        ImGui::Text("Time: %.2f ms", ts * 1000.f);
        if (ImGui::Button("Open Window"))
            GetApplication()->OpenWindow<ONYX::Orthographic2D>();

        ImGui::End();
    }

    bool OnEvent(const usize p_WindowIndex, const ONYX::Event &p_Event) noexcept override
    {
        if (p_Event.Type == ONYX::Event::MOUSE_PRESSED)
            GetApplication()->CloseWindow(p_WindowIndex);
        return false;
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