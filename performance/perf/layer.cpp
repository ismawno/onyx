#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "vulkan/vulkan_core.h"
#include <imgui.h>

namespace Onyx::Perf
{
template <Dimension D>
Layer<D>::Layer(Application *p_Application, const TKit::Span<const Lattice<D>> p_Lattices) noexcept
    : m_Application(p_Application), m_Lattices(p_Lattices.begin(), p_Lattices.end())
{
}

template <Dimension D> void Layer<D>::OnStart() noexcept
{
    m_Window = m_Application->GetMainWindow();
    m_Context = m_Window->CreateRenderContext<D>();
    m_Camera = m_Window->CreateCamera<D>();
    if constexpr (D == D3)
    {
        m_Camera->SetPerspectiveProjection();
        Transform<D3> transform{};
        transform.Translation = 3.f * fvec3{2.f, 0.75f, 2.f};
        transform.Rotation = glm::quat{glm::radians(fvec3{-15.f, 45.f, -4.f})};
        m_Camera->SetView(transform);
    }
    else
        m_Camera->SetSize(50.f);
    for (Lattice<D> &lattice : m_Lattices)
        if (lattice.Shape == ShapeType<D>::Mesh)
        {
            const auto result = Mesh<D>::Load(lattice.MeshPath);
            VKIT_ASSERT_RESULT(result);

            Mesh<D> mesh = result.GetValue();
            Onyx::Core::GetDeletionQueue().Push([mesh]() mutable { mesh.Destroy(); });
            lattice.Mesh = mesh;
        }
}
template <Dimension D> void Layer<D>::OnUpdate() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Perf::OnUpdate");
    const auto timestep = m_Application->GetDeltaTime();
    m_Camera->ControlMovementWithUserInput(3.f * timestep);
    if (ImGui::Begin("Info"))
        UserLayer::DisplayFrameTime(timestep);
    ImGui::End();
    // static bool first = false;
    // if (first)
    //     return;
    // first = true;

    m_Context->Flush();
    if constexpr (D == D3)
    {
        m_Context->Axes({.Thickness = 0.05f});
        m_Context->LightColor(Color::WHITE);
        m_Context->DirectionalLight(fvec3{1.f}, 0.55f);
    }

    m_Context->ShareCurrentState();
    for (const Lattice<D> &lattice : m_Lattices)
        lattice.Render(m_Context);
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

template class Layer<D2>;
template class Layer<D3>;
} // namespace Onyx::Perf
