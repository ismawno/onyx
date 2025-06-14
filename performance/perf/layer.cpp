#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "vulkan/vulkan_core.h"
#include <imgui.h>

namespace Onyx::Perf
{
template <Dimension D> Layer<D>::Layer(Application *p_Application) noexcept : m_Application(p_Application)
{
}

template <Dimension D> void Layer<D>::OnStart() noexcept
{
    m_Window = m_Application->GetMainWindow();
    m_Context = m_Window->CreateRenderContext<D>();
    m_Camera = m_Context->CreateCamera();
    if constexpr (D == D3)
    {
        m_Camera->SetPerspectiveProjection();
        Transform<D3> transform{};
        transform.Translation = {2.f, 0.75f, 2.f};
        transform.Rotation = glm::quat{glm::radians(fvec3{-15.f, 45.f, -4.f})};
        m_Camera->SetView(transform);
    }
}
template <Dimension D> void Layer<D>::OnRender(const u32, const VkCommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Perf::OnRender");
    const auto timestep = m_Application->GetDeltaTime();
    UserLayer::DisplayFrameTime(timestep);
    m_Context->Axes();
}
template <Dimension D> void Layer<D>::OnEvent(const Event &p_Event) noexcept
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;
    const f32 factor = Input::IsKeyPressed(p_Event.Window, Input::Key::LeftShift) && !ImGui::GetIO().WantCaptureKeyboard
                           ? 0.05f
                           : 0.005f;
    m_Camera->ControlScrollWithUserInput(factor * p_Event.ScrollOffset.y);
}
} // namespace Onyx::Perf
