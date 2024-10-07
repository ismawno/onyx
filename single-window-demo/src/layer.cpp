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
    context->ScaleAxes(0.3f);
    context->KeepWindowAspect();

    context->Fill(Color::CYAN);
    context->RoundedLine(0.f, 0.f, 4.f, 4.f);

    // context->TranslateAxes(0.2f, 0.f);
    // context->ScaleAxes(t);
    // context->Rotate(t);
    // context->Scale(0.4f);
    // context->Square(3.f, 0.f);

    // context->Push();
    // context->TranslateAxes(3.f, 0.f);
    // context->RotateAxes(t);
    // context->Square(0.f, 3.f);
    // context->Pop();

    // context->TranslateAxes(3.f, 5.f);
    // context->RotateAxes(t);
    // context->Square();

    // context->RotateView(t);
    // context->Square(0.f, 5.f);
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window"))
    {
        RenderContext2D *context = m_Application->GetMainWindow()->GetRenderContext2D();
        const vec2 mpos = context->GetMouseCoordinates();
        // const vec2 mpos = Input::GetMousePosition(m_Application->GetMainWindow());
        ImGui::Text("Mouse Position: (%.2f, %.2f)", mpos.x, mpos.y);
    }
    ImGui::End();
}

} // namespace ONYX