#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/imgui.hpp"

namespace Onyx::Demo
{
SWExampleLayer::SWExampleLayer(Application *p_Application, const Scene p_Scene)
    : m_Application(p_Application), m_Scene(p_Scene)
{
}

void SWExampleLayer::OnStart()
{
    m_Data.OnStart(m_Application->GetMainWindow(), m_Scene);
}

void SWExampleLayer::OnUpdate()
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

void SWExampleLayer::OnEvent(const Event &p_Event)
{
    m_Data.OnEvent(p_Event);
}

void SWExampleLayer::OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer)
{
    m_Data.OnRenderBegin(p_CommandBuffer);
}

} // namespace Onyx::Demo
