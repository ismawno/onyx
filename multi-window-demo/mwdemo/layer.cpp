#include "mwdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include <imgui.h>
#include <implot.h>

namespace Onyx::Demo
{
MWExampleLayer::MWExampleLayer(IMultiWindowApplication *p_Application) noexcept : m_Application(p_Application)
{
}

void MWExampleLayer::OnStart() noexcept
{
    for (u32 i = 0; i < m_Application->GetWindowCount(); ++i)
        m_Data[i].OnStart(m_Application->GetWindow(i));
}

void MWExampleLayer::OnUpdate(const u32 p_WindowIndex) noexcept
{
    m_Data[p_WindowIndex].OnUpdate();
}

bool MWExampleLayer::OnEvent(const u32 p_WindowIndex, const Event &p_Event) noexcept
{
    TKIT_ASSERT(p_Event.Type == Event::WindowOpened || p_WindowIndex < m_Data.size(), "[ONYX] Index out of bounds");
    if (p_Event.Type == Event::WindowOpened)
        m_Data.emplace_back().OnStart(p_Event.Window);
    else if (p_Event.Type == Event::WindowClosed)
        m_Data.erase(m_Data.begin() + p_WindowIndex);
    else
        m_Data[p_WindowIndex].OnEvent(p_Event);
    return true;
}

void MWExampleLayer::OnRender(const u32 p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
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