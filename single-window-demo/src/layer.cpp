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

void SWExampleLayer::OnStart() noexcept
{
    RenderContext3D *context = m_Application->GetMainWindow()->GetRenderContext3D();
    context->AddDirectionalLight({1.f, 0.f, 0.f}, 0.5f);
}

void SWExampleLayer::OnRender() noexcept
{
    RenderContext3D *context = m_Application->GetMainWindow()->GetRenderContext3D();
    static f32 t = 0.f;
    t += m_Application->GetDeltaTime();
    (void)t;
    context->GetDirectionalLight(0).z = -t;
    context->Background(Color::BLACK);
    context->KeepWindowAspect();
    context->Fill(Color::WHITE);
    // context->ScaleAxes(0.1f);
    context->Perspective(glm::radians(45.f), 1.f, 0.1f, 100.f);

    context->RotateX(0.5f);
    context->RotateY(0.5f);
    context->Sphere(-2.f, 0.f, 8.f);
    context->Cube(0.f, 0.f, 8.f);
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window"))
    {
        RenderContext3D *context = m_Application->GetMainWindow()->GetRenderContext3D();
        const vec3 mpos = context->GetMouseCoordinates(0.0f);
        // const vec2 mpos = Input::GetMousePosition(m_Application->GetMainWindow());
        ImGui::Text("Mouse Position: (%.2f, %.2f, %.2f)", mpos.x, mpos.y, mpos.z);
    }
    ImGui::End();
}

} // namespace ONYX