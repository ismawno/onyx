#include "sandbox/sandbox.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/imgui/imgui.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
SandboxAppLayer::SandboxAppLayer(const WindowLayers *layers) : ApplicationLayer(layers)
{
    AddMeshes<D2>();
    AddMeshes<D3>();

    RenderContext<D2> *ctx2 = AddContext<D2>();
    // RenderContext<D3> *ctx3 = AddContext<D3>();

    WindowSpecs wspecs{};
    wspecs.Title = "Onyx sandbox window (2D)";
    RequestOpenWindow<SandboxWinLayer>([ctx2](SandboxWinLayer *, Window *window) { ctx2->AddTarget(window); }, wspecs,
                                       D2);

    // wspecs.Name = "Onyx sandbox window (3D)";
    // RequestOpenWindow<SandboxWinLayer>([ctx3](SandboxWinLayer *, Window *window) { ctx3->AddTarget(window); },
    // wspecs,
    //                                    D3);
}
void SandboxAppLayer::OnTransfer(const DeltaTime &)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnTransfer");
    DrawShapes<D2>();
    DrawShapes<D3>();
}

template <Dimension D> void SandboxAppLayer::AddMeshes()
{
    Meshes &meshes = GetMeshes<D>();
    const auto addStatic = [&meshes](const char *name, const StatMeshData<D> &data) {
        const Mesh mesh = Assets::AddMesh(data);
        meshes.StaticMeshes.Append(name, mesh);
    };
    addStatic("Triangle", Assets::CreateTriangleMesh<D>());
    addStatic("Square", Assets::CreateSquareMesh<D>());
    if constexpr (D == D3)
    {
        addStatic("Cube", Assets::CreateCubeMesh());
        addStatic("Sphere", Assets::CreateSphereMesh(32, 64));
        addStatic("Cylinder", Assets::CreateCylinderMesh(64));
    }
    VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
}

template <Dimension D> static void setShapeProperties(RenderContext<D> *context, const Shape<D> &shape)
{
    context->Fill(shape.Flags & SandboxFlag_Fill);
    context->Fill(shape.FillColor);
    context->Outline(shape.Flags & SandboxFlag_Outline);
    context->Outline(shape.OutlineColor);
    context->OutlineWidth(shape.OutlineWidth);
}
template <Dimension D> static void drawShape(RenderContext<D> *context, const Shape<D> &shape)
{
    if (shape.Type.Geo == Geometry_Circle)
        context->Circle(shape.Transform.ComputeTransform());
    else
        context->StaticMesh(shape.Mesh, shape.Transform.ComputeTransform());
}
template <Dimension D> void SandboxAppLayer::DrawShapes()
{
    const auto &contexts = GetContexts<D>();
    for (const ContextData<D> &ctx : contexts.Contexts)
    {
        ctx.Context->Flush();
        for (const Shape<D> &shape : ctx.Shapes)
        {
            setShapeProperties(ctx.Context, shape);
            drawShape(ctx.Context, shape);
        }
        if (ctx.Flags & SandboxFlag_DrawAxes)
        {
            ctx.Context->Outline(false);
            ctx.Context->Fill(true);
            if constexpr (D == D2)
                ctx.Context->Axes(Meshes2.StaticMeshes[Mesh_Square].Mesh, {.Thickness = ctx.AxesThickness});
            else
                ctx.Context->Axes(Meshes3.StaticMeshes[Mesh_Cylinder].Mesh, {.Thickness = ctx.AxesThickness});
        }
        // if constexpr (D == D3)
        // {
        //     ctx.Context->AmbientColor(ctx.Ambient);
        //     for (const auto &light : ctx.DirLights)
        //         ctx.Context->DirectionalLight(light);
        //     for (const auto &light : ctx.PointLights)
        //     {
        //         if (ctx.Flags & SandboxFlag_DrawLights)
        //         {
        //             ctx.Context->Push();
        //             ctx.Context->Fill(Color::Unpack(light.Color));
        //             ctx.Context->Scale(0.01f);
        //             ctx.Context->Translate(light.Position);
        //             ctx.Context->StaticMesh(Meshes3.StaticMeshes[Mesh_Sphere].Mesh);
        //             ctx.Context->Pop();
        //         }
        //         ctx.Context->PointLight(light);
        //     }
        // };
    }
}
template <Dimension D> RenderContext<D> *SandboxAppLayer::AddContext()
{
    RenderContext<D> *context = Renderer::CreateContext<D>();
    auto &contexts = GetContexts<D>();
    ContextData<D> &data = contexts.Contexts.Append();
    data.Context = context;
    if constexpr (D == D3)
    {
        data.Flags = SandboxFlag_DrawAxes;
        data.DirLights.Append(f32v3{1.f, 1.f, 1.f}, 0.3f, Color::White.Pack());
    }
    return context;
}
SandboxWinLayer::SandboxWinLayer(ApplicationLayer *appLayer, Window *window, const Dimension dim)
    : WindowLayer(appLayer, window,
                  {.Flags = WindowLayerFlag_ImGuiEnabled, .ImGuiConfigFlags = ImGuiConfigFlags_ViewportsEnable})
{
    if (dim == D2)
        AddCamera<D2>();
    else
        AddCamera<D3>();
}

void SandboxWinLayer::OnRender(const DeltaTime &deltaTime)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnRender");
    if (!Cameras2.Cameras.IsEmpty())
        Cameras2.Cameras[Cameras2.Active].Camera->ControlMovementWithUserInput(deltaTime.Measured);
    if (!Cameras3.Cameras.IsEmpty())
        Cameras3.Cameras[Cameras3.Active].Camera->ControlMovementWithUserInput(deltaTime.Measured);

    RenderImGui();
}

void SandboxWinLayer::OnEvent(const Event &event)
{
    ProcessEvent<D2>(event);
    ProcessEvent<D3>(event);
}

#ifdef ONYX_ENABLE_IMGUI
void SandboxWinLayer::RenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::RenderImGui");
    ImGui::ShowDemoWindow();
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::ShowDemoWindow();
#    endif

    ApplicationLayer *appLayer = GetApplicationLayer();
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("App"))
        {
            if (ImGui::BeginMenu("New"))
            {
                if (ImGui::MenuItem("2D"))
                    appLayer->RequestOpenWindow<SandboxWinLayer>(D2);

                if (ImGui::MenuItem("3D"))
                    appLayer->RequestOpenWindow<SandboxWinLayer>(D3);

                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Reload ImGui"))
                RequestReloadImGui(GetImGuiConfigFlags());
            if (ImGui::MenuItem("Close"))
                RequestCloseWindow();
            if (ImGui::MenuItem("Quit"))
                appLayer->RequestQuitApplication();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    if (ImGui::Begin("Welcome to Onyx, my Vulkan application framework!"))
    {
        DeltaTimeEditor();
        const TKit::Timespan ts = appLayer->GetApplicationDeltaTime();
        ImGui::Text("Application delta time: %.2f ms", ts.AsMilliseconds());

        ImGui::Text("Version: " ONYX_VERSION);
        ImGui::TextWrapped(
            "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
            "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
            "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
            "The framework is still in its early stages, but I plan to expand it further in the future.");

        ImGui::TextWrapped("This program is the Onyx demo, showcasing some of its features. Most of them can be tried "
                           "in the 'Editor' panel.");

        ImGui::TextLinkOpenURL("My GitHub", "https://github.com/ismawno");

        ImGui::TextWrapped(
            "You may load meshes for this demo to use for both 2D and 3D. Take into account that meshes may "
            "have been created with a different coordinate "
            "system or unit scaling values. In Onyx, shapes with unit transforms are supposed to be centered around "
            "zero with a cartesian coordinate system and size (from end to end) of 1. That is why you may apply a "
            "transform before loading a specific mesh.");

        // if (ImGui::CollapsingHeader("2D Meshes"))
        //     renderMeshLoad<D2>(m_Meshes2, ONYX_ROOT_PATH "/demo/meshes2/");
        // if (ImGui::CollapsingHeader("3D Meshes"))
        //     renderMeshLoad<D3>(m_Meshes3, ONYX_ROOT_PATH "/demo/meshes3/");
    }
    ImGui::End();
}
#endif

template <Dimension D> void SandboxWinLayer::ProcessEvent(const Event &event)
{
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        return;
#endif
    const auto &cams = GetCameras<D>();
    if (cams.Cameras.IsEmpty())
        return;

    const CameraData<D> &cam = cams.Cameras[cams.Active];
    Camera<D> *camera = cam.Camera;
    if (event.Type == Event_Scrolled)
    {
        const f32 factor = Input::IsKeyPressed(GetWindow(), Input::Key_LeftShift) ? 0.05f : 0.005f;
        camera->ControlScrollWithUserInput(factor * event.ScrollOffset[1]);
    }
}
template <Dimension D> void SandboxWinLayer::AddCamera()
{
    Camera<D> *camera = GetWindow()->CreateCamera<D>();
    camera->BackgroundColor = Color{0.1f};

    auto &cameras = GetCameras<D>();
    CameraData<D> &data = cameras.Cameras.Append();
    data.Camera = camera;
    if constexpr (D == D3)
    {
        data.Perspective = true;
        data.Camera->SetPerspectiveProjection(data.FieldOfView, data.Near, data.Far);
        Transform<D3> transform{};
        transform.Translation = f32v3{2.f, 0.75f, 2.f};
        transform.Rotation = f32q{Math::Radians(f32v3{-15.f, 45.f, -4.f})};
        data.Camera->SetView(transform);
    }
}
} // namespace Onyx
