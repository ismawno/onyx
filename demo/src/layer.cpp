#include "demo/layer.hpp"
#include "onyx/app/app.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/camera/perspective.hpp"
#include <imgui.h>

namespace ONYX
{
enum CameraType : int
{
    ORTHOGRAPHIC2D = 0,
    ORTHOGRAPHIC3D,
    PERSPECTIVE3D
};

enum PrimitiveType : int
{
    RECTANGLE = 0
};

ExampleLayer::ExampleLayer() noexcept : Layer("Example")
{
}

void ExampleLayer::OnRender(const usize p_WindowIndex) noexcept
{
    IApplication *app = GetApplication();
    for (const auto &drawable : m_DrawObjects[p_WindowIndex])
        app->Draw(*drawable, p_WindowIndex);
}

void ExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window spawner"))
        renderWindowSpawner();
    ImGui::End();

    if (ImGui::Begin("Window controller"))
        renderWindowController();
    ImGui::End();
}

bool ExampleLayer::OnEvent(const usize, const Event &p_Event) noexcept
{
    if (p_Event.Type == Event::WINDOW_OPENED)
    {
        m_DrawObjects.emplace_back();
        return true;
    }
    return false;
}

void ExampleLayer::renderWindowSpawner() noexcept
{
    IApplication *app = GetApplication();
    static Window::Specs specs;
    static CameraType camera = ORTHOGRAPHIC2D;
    static f32 orthSize = 5.f;

    if (ImGui::Button("Open GLFW window"))
    {
        if (camera == ORTHOGRAPHIC2D)
            app->OpenWindow<Orthographic2D>(specs, orthSize);
        else if (camera == ORTHOGRAPHIC3D)
            app->OpenWindow<Orthographic3D>(specs, orthSize);
        else if (camera == PERSPECTIVE3D)
            app->OpenWindow<Perspective3D>(specs);
    }

    ImGui::Combo("Camera", (int *)&camera, "Orthographic2D\0Orthographic3D\0Perspective3D\0\0");
    if (camera < PERSPECTIVE3D)
        ImGui::DragFloat("Orthographic size", &orthSize, 0.5f, 0.f, FLT_MAX, "%.1f");

    ImGui::SliderInt2("Dimensions", (int *)&specs.Width, 120, 1080);
}

ONYX_DIMENSION_TEMPLATE static void renderTransform(Transform<N> &p_Transform) noexcept
{
    bool modified = false;
    if constexpr (N == 2)
    {
        modified |= ImGui::DragFloat2("Position", p_Transform.GetPositionPtr(), 0.1f);
        modified |= ImGui::DragFloat2("Scale", p_Transform.GetScalePtr(), 0.1f);
        modified |= ImGui::DragFloat2("Origin", p_Transform.GetOriginPtr(), 0.1f);
        modified |= ImGui::DragFloat("Rotation", p_Transform.GetRotationPtr(), 0.1f);
    }
    else
    {
        modified |= ImGui::DragFloat3("Position", p_Transform.GetPositionPtr());
        modified |= ImGui::DragFloat3("Scale", p_Transform.GetScalePtr());
        modified |= ImGui::DragFloat3("Origin", p_Transform.GetOriginPtr());
    }
    if (modified)
        p_Transform.UpdateMatricesAsModelForced();
}

ONYX_DIMENSION_TEMPLATE void ExampleLayer::renderObjectProperties(const usize p_WindowIndex) noexcept
{
    static Transform<N> transform;
    static PrimitiveType ptype = RECTANGLE;
    if (ImGui::Button("Spawn"))
    {
        if (ptype == RECTANGLE)
        {
            auto rect = KIT::Scope<ONYX::Rectangle<N>>::Create();
            rect->Transform = transform;
            m_DrawObjects[p_WindowIndex].emplace_back(std::move(rect));
        }
    }
    ImGui::Combo("Primitive", (int *)&ptype, "Rectangle\0\0");
    ImGui::Text("Transform: ");
    renderTransform(transform);
    if (ImGui::TreeNode("Active primitives"))
    {
        for (const auto &drawable : m_DrawObjects[p_WindowIndex])
        {
            // This is awful. It is also a demo
            IShape<N> *shape = dynamic_cast<IShape<N> *>(drawable.Get());
            if (!shape)
                continue;
            renderTransform(shape->Transform);
        }
        ImGui::TreePop();
    }
}

void ExampleLayer::renderWindowController() noexcept
{
    IApplication *app = GetApplication();
    for (usize i = 0; i < app->GetWindowCount(); ++i)
    {
        const Window *window = app->GetWindow(i);
        if (ImGui::TreeNode(window, "Window %zu", i))
        {
            ImGui::Text("2D Primitives");
            renderObjectProperties<2>(i);

            ImGui::Text("3D Primitives");
            renderObjectProperties<3>(i);
            ImGui::TreePop();
        }
    }
}

} // namespace ONYX