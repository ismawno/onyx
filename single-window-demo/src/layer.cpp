#include "swdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{
enum PrimitiveType : int
{
    TRIANGLE = 0,
    RECTANGLE,
    CIRCLE,
    CUBE,
    SPHERE
};

SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnRender() noexcept
{
    RenderContext2D *context = m_Application->GetMainWindow()->GetRenderContext2D();
    static f32 t = 0.f;
    t += m_Application->GetDeltaTime();
    (void)t;
    context->Background(Color::BLACK);
    context->ScaleView(10.f);
    context->KeepWindowAspect();

    context->Translate(0.f, 10.f);
    // context->Scale(0.4f);
    // context->RotateView(t);
    context->Square();

    // context->RotateView(t);
    // context->Square(0.f, 5.f);
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window"))
    {
        RenderContext2D *context = m_Application->GetMainWindow()->GetRenderContext2D();
        const vec2 mpos = context->GetViewMousePosition();
        ImGui::Text("Mouse Position: (%.1f, %.1f)", mpos.x, mpos.y);
    }
    ImGui::End();
}

} // namespace ONYX