#include "mwdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{
MWExampleLayer::MWExampleLayer(IMultiWindowApplication *p_Application) noexcept
    : Layer("Example"), m_Application(p_Application)
{
}

void MWExampleLayer::OnStart() noexcept
{
    for (usize i = 0; i < m_Application->GetWindowCount(); ++i)
        m_Data[i].OnStart(m_Application->GetWindow(i));
}

bool MWExampleLayer::OnEvent(const usize p_WindowIndex, const Event &p_Event) noexcept
{
    KIT_ASSERT(p_Event.Type == Event::WindowOpened || p_WindowIndex < m_Data.size(), "Index out of bounds");
    if (p_Event.Type == Event::WindowOpened)
        m_Data.emplace_back().OnStart(p_Event.Window);
    else if (p_Event.Type == Event::WindowClosed)
        m_Data.erase(m_Data.begin() + p_WindowIndex);
    else
        m_Data[p_WindowIndex].OnEvent(p_Event);
    return true;
}

void MWExampleLayer::OnRender(const usize p_WindowIndex, const VkCommandBuffer) noexcept
{
    m_Data[p_WindowIndex].OnRender();
}

void MWExampleLayer::OnImGuiRender() noexcept
{
    const auto ts = m_Application->GetDeltaTime();
    WindowData::OnImGuiRenderGlobal(ts);
    if (ImGui::Begin("Editor"))
    {
        if (ImGui::Button("Open Window"))
            m_Application->OpenWindow();

        for (usize i = 0; i < m_Application->GetWindowCount(); ++i)
        {
            Window *window = m_Application->GetWindow(i);
            if (ImGui::TreeNode(window, window->GetName(), i))
            {
                m_Data[i].OnImGuiRender(ts);
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

} // namespace ONYX