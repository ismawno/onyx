#include "swdemo/layer.hpp"
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

SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    ImGui::ShowDemoWindow();
    ImPlot::ShowDemoWindow();
    if (ImGui::Begin("Test"))
    {
        if (ImGui::Button("Shutdown!"))
            m_Application->Shutdown();
    }
    ImGui::End();
}

} // namespace ONYX