#include "mwdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include "onyx/camera/orthographic.hpp"
#include "onyx/camera/perspective.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{
enum PrimitiveType : int
{
    RECTANGLE = 0
};

MWExampleLayer::MWExampleLayer(IMultiWindowApplication *p_Application) noexcept
    : Layer("Example"), m_Application(p_Application)
{
}

void MWExampleLayer::OnRender(const usize p_WindowIndex) noexcept
{
    for (const auto &drawable : m_WindowData[p_WindowIndex].Drawables)
        m_Application->Draw(*drawable, p_WindowIndex);
}

void MWExampleLayer::OnImGuiRender() noexcept
{
    ImGui::ShowDemoWindow();
    ImPlot::ShowDemoWindow();
    if (ImGui::Begin("Window spawner"))
        renderWindowSpawner();
    ImGui::End();

    if (ImGui::Begin("Window controller"))
        renderWindowController();
    ImGui::End();
}

bool MWExampleLayer::OnEvent(const usize, const Event &p_Event) noexcept
{
    if (p_Event.Type == Event::WINDOW_OPENED)
    {
        m_WindowData.emplace_back();
        return true;
    }
    return false;
}

void MWExampleLayer::renderWindowSpawner() noexcept
{
    static Window::Specs specs;
    static CameraType camera = ORTHOGRAPHIC2D;
    static f32 orthSize = 5.f;

    if (ImGui::Button("Open GLFW window"))
    {
        if (camera == ORTHOGRAPHIC2D)
            m_Application->OpenWindow<Orthographic2D>(specs, orthSize);
        else if (camera == ORTHOGRAPHIC3D)
            m_Application->OpenWindow<Orthographic3D>(specs, orthSize);
        else if (camera == PERSPECTIVE3D)
            m_Application->OpenWindow<Perspective3D>(specs);
    }

    ImGui::Combo("Camera", (int *)&camera, "Orthographic2D\0Orthographic3D\0Perspective3D\0\0");
    if (camera < PERSPECTIVE3D)
        ImGui::DragFloat("Orthographic size", &orthSize, 0.5f, 0.f, FLT_MAX, "%.1f");

    ImGui::SliderInt2("Dimensions", (int *)&specs.Width, 120, 1080);
}

template <u32 N>
    requires(IsDim<N>())
static void renderTransform(Transform<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
    {
        ImGui::DragFloat2("Position", glm::value_ptr(p_Transform.Position), 0.1f);
        ImGui::DragFloat2("Scale", glm::value_ptr(p_Transform.Scale), 0.1f);
        ImGui::DragFloat2("Origin", glm::value_ptr(p_Transform.Origin), 0.1f);
        ImGui::DragFloat("Rotation", &p_Transform.Rotation, 0.1f);
    }
    else
    {
        ImGui::DragFloat3("Position", glm::value_ptr(p_Transform.Position));
        ImGui::DragFloat3("Scale", glm::value_ptr(p_Transform.Scale));
        ImGui::DragFloat3("Origin", glm::value_ptr(p_Transform.Origin));
    }
}

template <u32 N>
    requires(IsDim<N>())
void MWExampleLayer::renderObjectProperties(const usize p_WindowIndex) noexcept
{
    static PrimitiveType ptype = RECTANGLE;
    if (ImGui::Button("Spawn"))
    {
        if (ptype == RECTANGLE)
            m_WindowData[p_WindowIndex].Drawables.emplace_back(KIT::Scope<ONYX::Rectangle<N>>::Create());
    }
    ImGui::Combo("Primitive", (int *)&ptype, "Rectangle\0\0");
    if (ImGui::TreeNode("Active primitives"))
    {
        for (const auto &drawable : m_WindowData[p_WindowIndex].Drawables)
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

void MWExampleLayer::renderWindowController() noexcept
{
    for (usize i = 0; i < m_Application->GetWindowCount(); ++i)
    {
        const Window *window = m_Application->GetWindow(i);
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