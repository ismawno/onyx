#include "swdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/camera/perspective.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{
enum PrimitiveType : int
{
    TRIANGLE = 0,
    RECTANGLE,
    CIRCLE
};

SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnRender() noexcept
{
    for (const auto &drawable : m_Drawables)
        m_Application->Draw(*drawable);
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window"))
    {
        static f32 orthSize = 5.f;

        Window *window = m_Application->GetMainWindow();
        ICamera *camera = window->GetCamera();

        const vec2 mpos = camera->ComputeInverseProjectionView() * vec4{Input::GetMousePosition(window), 0.f, 1.f};
        ImGui::Text("Mouse position: (%.2f, %.2f)", mpos.x, mpos.y);
        if (m_CameraType < PERSPECTIVE3D)
            ImGui::DragFloat("Orthographic size", &orthSize, 0.5f, 0.f, FLT_MAX, "%.1f");

        if (ImGui::Combo("Camera type", (int *)&m_CameraType, "Orthographic 2D\0Orthographic 3D\0Perspective 3D\0\0"))
        {
            switch (m_CameraType)
            {
            case ORTHOGRAPHIC2D:
                window->SetCamera<Orthographic2D>(orthSize);
                m_CameraType = ORTHOGRAPHIC2D;
                break;
            case ORTHOGRAPHIC3D:
                window->SetCamera<Orthographic3D>(orthSize);
                m_CameraType = ORTHOGRAPHIC3D;
                break;
            case PERSPECTIVE3D:
                window->SetCamera<Perspective3D>();
                m_CameraType = PERSPECTIVE3D;
                break;
            }
        }
        if (ImGui::Button("Clear primitives"))
            m_Drawables.clear();

        static Color color = Color::RED;
        if (ImGui::CollapsingHeader("Color picker"))
            ImGui::ColorPicker3("Color", color.AsPointer());
        renderPrimitiveSpawnUI<2>(color);
        renderPrimitiveSpawnUI<3>(color);
    }
    ImGui::End();
}

ONYX_DIMENSION_TEMPLATE static void renderTransform(Transform<N> &p_Transform) noexcept
{
    ImGui::PushID(&p_Transform);
    if constexpr (N == 2)
    {
        ImGui::DragFloat2("Position", glm::value_ptr(p_Transform.Position), 0.1f);
        ImGui::DragFloat2("Scale", glm::value_ptr(p_Transform.Scale), 0.1f);
        ImGui::DragFloat2("Origin", glm::value_ptr(p_Transform.Origin), 0.1f);
        ImGui::DragFloat("Rotation", &p_Transform.Rotation, 0.1f);
        vec2 offset{0.f};
        if (ImGui::DragFloat2("Translate locally", glm::value_ptr(offset), 0.1f, 0.f, 0.f, "Slide!"))
            p_Transform.Position += p_Transform.LocalOffset(offset);
    }
    else
    {
        ImGui::DragFloat3("Position", glm::value_ptr(p_Transform.Position), 0.1f);
        ImGui::DragFloat3("Scale", glm::value_ptr(p_Transform.Scale), 0.1f);
        ImGui::DragFloat3("Origin", glm::value_ptr(p_Transform.Origin), 0.1f);
        vec3 angles{0.f};
        if (ImGui::DragFloat3("Rotate (Local)", glm::value_ptr(angles), 0.1f, 0.f, 0.f, "Slide!"))
            p_Transform.RotateLocalAxis(angles);

        if (ImGui::DragFloat3("Rotate (Global)", glm::value_ptr(angles), 0.1f, 0.f, 0.f, "Slide!"))
            p_Transform.RotateGlobalAxis(angles);

        vec3 offset{0.f};
        if (ImGui::DragFloat3("Translate locally", glm::value_ptr(offset), 0.1f, 0.f, 0.f, "Slide!"))
            p_Transform.Position += p_Transform.LocalOffset(offset);
    }
    ImGui::PopID();
}

ONYX_DIMENSION_TEMPLATE void SWExampleLayer::renderPrimitiveSpawnUI(const Color &p_Color) noexcept
{
    ImGui::PushID(N);
    static PrimitiveType ptype = RECTANGLE;
    if (ImGui::Button("Spawn"))
    {
        if (ptype == TRIANGLE)
            m_Drawables.emplace_back(KIT::Scope<Triangle<N>>::Create(p_Color));
        if (ptype == RECTANGLE)
            m_Drawables.emplace_back(KIT::Scope<Rectangle<N>>::Create(p_Color));
        if (ptype == CIRCLE)
            m_Drawables.emplace_back(KIT::Scope<Ellipse<N>>::Create(p_Color));
    }
    ImGui::Combo("Primitive", (int *)&ptype, "Triangle\0Rectangle\0Circle\0\0");
    if (ImGui::TreeNode("Active primitives"))
    {
        for (const auto &drawable : m_Drawables)
        {
            // This is awful. It is also a demo
            IShape<N> *shape = dynamic_cast<IShape<N> *>(drawable.Get());
            if (!shape)
                continue;
            renderTransform(shape->Transform);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

} // namespace ONYX