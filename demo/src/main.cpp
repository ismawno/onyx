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
        static float time = 0.f;
        const float ts = GetApplication()->GetDeltaTime();
        time += ts;
        if (time < 1.f)
            m_Rectangles[p_WindowIndex].Transform.RotateLocalY(ts);
        else
            m_Rectangles[p_WindowIndex].Transform.RotateLocalZ(ts);

        GetApplication()->Draw(m_Rectangles[p_WindowIndex], p_WindowIndex);

        // window->GetCamera<ONYX::Perspective3D>()->Transform.Rotation += ts;
    }

    void OnImGuiRender() noexcept override
    {
        const float ts = GetApplication()->GetDeltaTime();
        ImGui::Begin("Example Layer");
        ImGui::Text("Hello, World!");
        ImGui::Text("Time: %.2f ms", ts * 1000.f);
        if (ImGui::Button("Open Window"))
            GetApplication()->OpenWindow<ONYX::Orthographic3D>();

        ImGui::End();
    }

    bool OnEvent(const usize, const ONYX::Event &p_Event) noexcept override
    {
        if (p_Event.Type == ONYX::Event::WINDOW_OPENED)
        {
            auto &rect = m_Rectangles.emplace_back(ONYX::Color::GREEN);
            // rect.Transform.RotateLocalZ(1.2f);
            // rect.Transform.RotateLocalY(1.2f);
            rect.Transform.Position.z = 5.f;
            return true;
        }
        return false;
    }

  private:
    KIT::StaticArray<ONYX::Rectangle3D, 12> m_Rectangles;
};

int main()
{
    KIT::StackAllocator stackAllocator(10_kb);
    KIT::ThreadPool<KIT::SpinLock> threadPool(4);
    ONYX::Core::Initialize(&stackAllocator, &threadPool);

    ONYX::Application<ONYX::MultiWindowFlow::CONCURRENT> app;
    app.Layers.Push<ExampleLayer>("Example Layer");

    // app.OpenWindow<ONYX::Perspective3D>();
    app.OpenWindow<ONYX::Perspective3D>();
    // auto win = app.OpenWindow<ONYX::Orthographic3D>();
    // win->GetCamera<ONYX::Orthographic3D>()->SetSize(10.f);

    app.Run();

    ONYX::Core::Terminate();
}