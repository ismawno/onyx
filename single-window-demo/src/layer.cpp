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
    TRIANGLE = 0,
    RECTANGLE,
    CIRCLE,
    CUBE,
    SPHERE
};

SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnRender() noexcept
{
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Window"))
    {
    }
    ImGui::End();
}

} // namespace ONYX