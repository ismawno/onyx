#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/imgui.hpp"

namespace Onyx::Demo
{
template <Dimension D>
Layer<D>::Layer(Application *p_Application, Window *p_Window, const TKit::Span<const Lattice<D>> p_Lattices)
    : UserLayer(p_Application, p_Window), m_Lattices(p_Lattices.begin(), p_Lattices.end())
{
    m_Window = m_Application->GetMainWindow();
    m_Context = m_Window->CreateRenderContext<D>();
    m_Camera = m_Window->CreateCamera<D>();
    if constexpr (D == D3)
    {
        m_Camera->SetPerspectiveProjection();
        Transform<D3> transform{};
        transform.Translation = 3.f * f32v3{2.f, 0.75f, 2.f};
        transform.Rotation = f32q{Math::Radians(f32v3{-15.f, 45.f, -4.f})};
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

template <Dimension D> void Layer<D>::OnUpdate()
{
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnUpdate");
    const auto timestep = m_Application->GetDeltaTime();
    m_Camera->ControlMovementWithUserInput(3.f * timestep);
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::Begin("Info"))
        UserLayer::DisplayFrameTime(timestep);
    ImGui::Text("Version: " ONYX_VERSION);
    ImGui::End();
#endif
    // static bool first = false;
    // if (first)
    //     return;
    // first = true;

    m_Context->Flush();
    if constexpr (D == D3)
    {
        m_Context->Axes({.Thickness = 0.05f});
        m_Context->LightColor(Color::WHITE);
        m_Context->DirectionalLight(f32v3{1.f}, 0.55f);
    }

    for (const Lattice<D> &lattice : m_Lattices)
        lattice.Render(m_Context);
}
template <Dimension D> void Layer<D>::OnEvent(const Event &p_Event)
{
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        return;
#endif
    const f32 factor = Input::IsKeyPressed(p_Event.Window, Input::Key::LeftShift) ? 0.05f : 0.005f;
    m_Camera->ControlScrollWithUserInput(factor * p_Event.ScrollOffset[1]);
}

template class Layer<D2>;
template class Layer<D3>;
} // namespace Onyx::Demo
