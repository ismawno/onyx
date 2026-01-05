#include "perf/layer.hpp"
#include "onyx/app/app.hpp"
#include "onyx/imgui/imgui.hpp"

namespace Onyx::Demo
{
template <Dimension D>
Layer<D>::Layer(Application *p_Application, Window *p_Window, const Lattice<D> &p_Lattice,
                const ShapeSettings &p_Options)
    : UserLayer(p_Application, p_Window), m_Lattice(p_Lattice), m_Options(p_Options)
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

    switch (p_Options.Shape)
    {
    case Shape::Triangle:
        m_Mesh = Assets::AddMesh(Assets::CreateTriangleMesh<D>());
        break;
    case Shape::Square:
        m_Mesh = Assets::AddMesh(Assets::CreateSquareMesh<D>());
        break;
    case Shape::NGon:
        m_Mesh = Assets::AddMesh(Assets::CreateRegularPolygonMesh<D>(p_Options.NGonSides));
        break;
    case Shape::Polygon:
        m_Mesh = Assets::AddMesh(Assets::CreatePolygonMesh<D>(p_Options.PolygonVertices));
        break;
    case Shape::ImportedStatic: {
        const auto result = Assets::LoadStaticMesh<D>(p_Options.MeshPath.c_str());
        VKIT_CHECK_RESULT(result);
        m_Mesh = Assets::AddMesh(result.GetValue());
        break;
    }
    default:
        break;
    }
    if constexpr (D == D3)
        switch (p_Options.Shape)
        {
        case Shape::Cube:
            m_Mesh = Assets::AddMesh(Assets::CreateCubeMesh());
            break;
        case Shape::Sphere:
            m_Mesh = Assets::AddMesh(Assets::CreateSphereMesh(p_Options.SphereRings, p_Options.SphereSectors));
            break;
        case Shape::Cylinder:
            m_Mesh = Assets::AddMesh(Assets::CreateCylinderMesh(p_Options.CylinderSides));
            break;
        default:
            break;
        }

    if constexpr (D == D3)
        m_AxesMesh = p_Options.Shape == Shape::Cylinder ? m_Mesh : Assets::AddMesh(Assets::CreateCylinderMesh(16));
    Assets::Upload<D>();
}

template <Dimension D> void Layer<D>::OnFrameBegin(const DeltaTime &p_DeltaTime, const FrameInfo &)
{
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnFrameBegin");
    const TKit::Timespan timestep = p_DeltaTime.Measured;
    m_Camera->ControlMovementWithUserInput(3.f * timestep);
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::Begin("Info"))
        m_Application->DisplayDeltaTime(UserLayerFlag_DisplayHelp);
    ImGui::Text("Version: " ONYX_VERSION);
    ImGui::End();
#endif
    // static bool first = false;
    // if (first)
    //     return;
    // first = true;

    m_Context->Flush();
    m_Context->ShareCurrentState();
    if constexpr (D == D3)
    {
        m_Context->Axes(m_AxesMesh);
        m_Context->LightColor(Color::WHITE);
        m_Context->DirectionalLight(f32v3{1.f}, 0.55f);
    }

    switch (m_Options.Shape)
    {
    case Shape::Circle:
        m_Lattice.Circle(m_Context, m_Options.CircleOptions);
        break;
    default:
        m_Lattice.StaticMesh(m_Context, m_Mesh);
        break;
    }
}
template <Dimension D> void Layer<D>::OnEvent(const Event &p_Event)
{
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        return;
#endif
    const f32 factor = Input::IsKeyPressed(m_Window, Input::Key_LeftShift) ? 0.05f : 0.005f;
    m_Camera->ControlScrollWithUserInput(factor * p_Event.ScrollOffset[1]);
}

template class Layer<D2>;
template class Layer<D3>;
} // namespace Onyx::Demo
