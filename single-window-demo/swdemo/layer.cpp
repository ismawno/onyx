#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include <imgui.h>
#include <implot.h>

namespace Onyx::Demo
{
SWExampleLayer::SWExampleLayer(Application *p_Application, const Scene p_Scene) noexcept
    : m_Application(p_Application), m_Scene(p_Scene)
{
}

void SWExampleLayer::OnStart() noexcept
{
    m_Data.OnStart(m_Application->GetMainWindow(), m_Scene);
}

void SWExampleLayer::OnUpdate() noexcept
{
    const auto ts = m_Application->GetDeltaTime();
    m_Data.OnUpdate(ts);
    WindowData::OnImGuiRenderGlobal(ts);
    if (ImGui::Begin("Editor"))
    {
        WindowData::RenderEditorText();
        m_Data.OnImGuiRender();
    }
    ImGui::End();
}

void SWExampleLayer::OnEvent(const Event &p_Event) noexcept
{
    m_Data.OnEvent(p_Event);
}

void SWExampleLayer::OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer) noexcept
{
    m_Data.OnRenderBegin(p_CommandBuffer);
}

} // namespace Onyx::Demo
