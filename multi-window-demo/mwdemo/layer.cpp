#include "mwdemo/layer.hpp"
#include <imgui.h>
#include <implot.h>

namespace Onyx::Demo
{
MWExampleLayer::MWExampleLayer(MultiWindowApplication *p_Application, const Scene p_Scene) noexcept
    : m_Application(p_Application), m_Scene(p_Scene)
{
}

void MWExampleLayer::OnUpdate(const u32 p_WindowIndex) noexcept
{
    m_Data[p_WindowIndex].OnUpdate();
}

void MWExampleLayer::OnEvent(const u32 p_WindowIndex, const Event &p_Event) noexcept
{
    TKIT_ASSERT(p_Event.Type == Event::WindowOpened || p_WindowIndex < m_Data.GetSize(), "[ONYX] Index out of bounds");

    if (p_Event.Type == Event::WindowOpened)
        m_Data.Append().OnStart(p_Event.Window, m_Scene);
    else if (p_Event.Type == Event::WindowClosed)
        m_Data.RemoveOrdered(m_Data.begin() + p_WindowIndex);
    else
        m_Data[p_WindowIndex].OnEvent(p_Event);
}

void MWExampleLayer::OnRender(const u32 p_WindowIndex, u32, const VkCommandBuffer p_CommandBuffer) noexcept
{
    const auto ts = m_Application->GetDeltaTime();
    m_Data[p_WindowIndex].OnRender(p_CommandBuffer, ts);
}

void MWExampleLayer::OnImGuiRender() noexcept
{
    const auto ts = m_Application->GetDeltaTime();
    WindowData::OnImGuiRenderGlobal(ts);
    if (ImGui::Begin("Editor"))
    {
        WindowData::RenderEditorText();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "This is a multi-window application, meaning windows can be opened and closed at runtime. "
            "The editor panel is shared between all windows, and each window has its own set of 2D or 3D shapes.");

        i32 scene = static_cast<i32>(m_Scene);
        if (ImGui::Combo("Scene setup", &scene,
                         "None\0"
                         "2D\0"
                         "3D\0\0"))
            m_Scene = static_cast<Scene>(scene);

        if (ImGui::Button("Open Window"))
            m_Application->OpenWindow();

        for (u32 i = 0; i < m_Application->GetWindowCount(); ++i)
        {
            Window *window = m_Application->GetWindow(i);
            if (ImGui::TreeNode(window, window->GetName(), i))
            {
                m_Data[i].OnImGuiRender();
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

} // namespace Onyx::Demo
