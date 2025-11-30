#include "sbox/layer.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/imgui.hpp"

namespace Onyx::Demo
{
SandboxLayer::SandboxLayer(SingleWindowApp *p_Application, const Dimension p_Dim)
    : m_SingleApp(p_Application), m_SingleData(p_Application->GetMainWindow(), p_Dim), m_Dim(p_Dim)
{
}

void SandboxLayer::OnUpdate()
{
    const auto ts = m_SingleApp->GetDeltaTime();
    m_SingleData.OnUpdate(ts);
    QuitResult = WindowData::OnImGuiRenderGlobal(m_SingleApp, ts, ApplicationType::SingleWindow);
    if (ImGui::Begin("Editor"))
    {
        WindowData::RenderEditorText();
        m_SingleData.OnImGuiRender();
    }
    ImGui::End();
}

void SandboxLayer::OnEvent(const Event &p_Event)
{
    m_SingleData.OnEvent(p_Event);
}

void SandboxLayer::OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer)
{
    m_SingleData.OnRenderBegin(p_CommandBuffer);
}

SandboxLayer::SandboxLayer(MultiWindowApp *p_Application, const Dimension p_Dim)
    : m_MultiApp(p_Application), m_Dim(p_Dim)
{
}

void SandboxLayer::OnUpdate(const u32 p_WindowIndex)
{
    const auto ts = m_MultiApp->GetDeltaTime();
    m_MultiData[p_WindowIndex].OnUpdate(ts);
}

void SandboxLayer::OnEvent(const u32 p_WindowIndex, const Event &p_Event)
{
    if (p_Event.Type == Event::WindowOpened)
        m_MultiData.Append(p_Event.Window, m_Dim);
    else if (p_Event.Type == Event::WindowClosed)
        m_MultiData.RemoveOrdered(m_MultiData.begin() + p_WindowIndex);
    else
        m_MultiData[p_WindowIndex].OnEvent(p_Event);
}

void SandboxLayer::OnRenderBegin(const u32 p_WindowIndex, u32, VkCommandBuffer p_CommandBuffer)
{
    m_MultiData[p_WindowIndex].OnRenderBegin(p_CommandBuffer);
}

void SandboxLayer::OnImGuiRender()
{
    const auto ts = m_MultiApp->GetDeltaTime();
    QuitResult = WindowData::OnImGuiRenderGlobal(m_MultiApp, ts, ApplicationType::MultiWindow);
    if (ImGui::Begin("Editor"))
    {
        WindowData::RenderEditorText();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "This is a multi-window application, meaning windows can be opened and closed at runtime. "
            "The editor panel is shared between all windows, and each window has its own set of 2D or 3D shapes.");

        i32 scene = static_cast<i32>(m_Dim) - 2;
        if (ImGui::Combo("Scene setup", &scene,
                         "2D\0"
                         "3D\0\0"))
            m_Dim = static_cast<Dimension>(scene + 2);

        if (ImGui::Button("Open Window"))
            m_MultiApp->OpenWindow();

        for (u32 i = 0; i < m_MultiApp->GetWindowCount(); ++i)
        {
            Window *window = m_MultiApp->GetWindow(i);
            if (ImGui::TreeNode(window, window->GetName(), i))
            {
                m_MultiData[i].OnImGuiRender();
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

} // namespace Onyx::Demo
