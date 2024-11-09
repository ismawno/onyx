#include "swdemo/layer.hpp"
#include "onyx/app/app.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{
SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnStart() noexcept
{
    m_Data.OnStart(m_Application->GetMainWindow());
}

bool SWExampleLayer::OnEvent(const Event &p_Event) noexcept
{
    Window *window = m_Application->GetMainWindow();
    m_Data.OnEvent(p_Event, window);
    return true;
}

void SWExampleLayer::OnRender() noexcept
{
    m_Data.OnRender();
    const auto ts = m_Application->GetDeltaTime();
    WindowData::OnImGuiRenderGlobal(ts);
    if (ImGui::Begin("Editor"))
        m_Data.OnImGuiRender(ts, m_Application->GetMainWindow());
    ImGui::End();
}

} // namespace ONYX