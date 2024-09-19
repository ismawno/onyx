#include "onyx/app/app.hpp"
#include "onyx/core/core.hpp"
#include "onyx/app/input.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/camera/perspective.hpp"
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
        const float ts = GetApplication()->GetDeltaTime();
        static ONYX::Rectangle3D rect(ONYX::Color::RED);
        rect.Transform.Position.z = 5.f;
        rect.Transform.RotateLocal(ONYX::Transform3D::RotY(ts));

        auto window = GetApplication()->GetWindow(p_WindowIndex);
        window->Draw(rect);
        window->GetCamera<ONYX::Perspective3D>()->Transform.Scale += vec3(ts);

        // window->GetCamera<ONYX::Perspective3D>()->Transform.Rotation += ts;
    }

    void OnImGuiRender() noexcept override
    {
        const float ts = GetApplication()->GetDeltaTime();
        ImGui::Begin("Example Layer");
        ImGui::Text("Hello, World!");
        ImGui::Text("Time: %.2f ms", ts * 1000.f);
        // if (ImGui::Button("Open Window"))
        //     GetApplication()->OpenWindow<ONYX::Perspective3D>();

        ImGui::End();
    }
};

int main()
{
    KIT::StackAllocator stackAllocator(10_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application<ONYX::MultiWindowFlow::SERIAL> app;
    app.Layers.Push<ExampleLayer>("Example Layer");

    // app.OpenWindow<ONYX::Perspective3D>();
    app.OpenWindow<ONYX::Perspective3D>(16.f / 9.f);
    auto win = app.OpenWindow<ONYX::Orthographic3D>();
    win->GetCamera<ONYX::Orthographic3D>()->SetSize(10.f);

    app.Run();

    ONYX::Core::Terminate();
}