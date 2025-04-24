#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include <imgui.h>
#include <implot.h>

namespace Onyx::Demo
{
SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : m_Application(p_Application)
{
}

void SWExampleLayer::OnStart() noexcept
{
    m_Data.OnStart(m_Application->GetMainWindow());
}

void SWExampleLayer::OnUpdate() noexcept
{
    m_Data.OnUpdate();
}

void SWExampleLayer::OnEvent(const Event &p_Event) noexcept
{
    m_Data.OnEvent(p_Event);
}

void SWExampleLayer::OnRender(u32, const VkCommandBuffer p_CommandBuffer) noexcept
{
    const auto ts = m_Application->GetDeltaTime();
    m_Data.OnRender(p_CommandBuffer, ts);
    WindowData::OnImGuiRenderGlobal(ts);
    if (ImGui::Begin("Editor"))
    {
        WindowData::RenderEditorText();
        m_Data.OnImGuiRender();
    }
    ImGui::End();
}

} // namespace Onyx::Demo