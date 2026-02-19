#include "sandbox/sandbox.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/platform/dialog.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{

// black magic
template <typename C, typename F1, typename F2>
static void renderSelectableNoRemoval(const char *treeName, C &container, u32 &selected, F1 onSelected, F2 getName)
{
    renderSelectable(treeName, container, selected, onSelected, [](const auto &) {}, getName);
}

template <typename C, typename F1, typename F2>
static void renderSelectableNoTree(const char *elementName, C &container, u32 &selected, F1 onSelected, F2 onRemoval)
{
    renderSelectable(nullptr, container, selected, onSelected, onRemoval,
                     [elementName](const auto &) { return elementName; });
}

template <typename C, typename F1, typename F2, typename F3>
static void renderSelectable(const char *treeName, C &container, u32 &selected, F1 onSelected, F2 onRemoval, F3 getName)
{
    if (!container.IsEmpty() && (!treeName || ImGui::TreeNode(treeName)))
    {
        for (u32 i = 0; i < container.GetSize(); ++i)
        {
            ImGui::PushID(&container[i]);
            if (ImGui::Button("X"))
            {
                onRemoval(container[i]);
                container.RemoveOrdered(container.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            std::string name;
            if constexpr (std::is_same_v<F3, const char *>)
                name = getName;
            else
                name = getName(container[i]);

            if (ImGui::Selectable(name.c_str(), i == selected))
                selected = i;

            ImGui::PopID();
        }
        if (selected < container.GetSize())
            onSelected(container[selected]);
        if (treeName)
            ImGui::TreePop();
    }
}

SandboxAppLayer::SandboxAppLayer(const WindowLayers *layers) : ApplicationLayer(layers)
{
    AddMeshes<D2>();
    AddMeshes<D3>();

    WindowSpecs wspecs{};
    wspecs.Title = "Onyx sandbox window (2D)";
    RequestOpenWindow<SandboxWinLayer>(wspecs, D2);

    wspecs.Title = "Onyx sandbox window (3D)";
    RequestOpenWindow<SandboxWinLayer>(wspecs, D3);
}
void SandboxAppLayer::OnTransfer(const DeltaTime &)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnTransfer");
    DrawShapes<D2>();
    DrawShapes<D3>();
}

template <Dimension D> void SandboxAppLayer::AddStaticMesh(const char *name, const StatMeshData<D> &data)
{
    Meshes<D> &meshes = GetMeshes<D>();
    const Mesh mesh = Assets::AddMesh(data);
    meshes.StaticMeshes.Append(name, mesh);
}
template <Dimension D> void SandboxAppLayer::AddMeshes()
{
    AddStaticMesh("Triangle", Assets::CreateTriangleMesh<D>());
    AddStaticMesh("Square", Assets::CreateSquareMesh<D>());
    if constexpr (D == D3)
    {
        AddStaticMesh("Cube", Assets::CreateCubeMesh());
        AddStaticMesh("Sphere", Assets::CreateSphereMesh(32, 64));
        AddStaticMesh("Cylinder", Assets::CreateCylinderMesh(64));
    }
    VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
}

template <Dimension D> static void setShapeProperties(RenderContext<D> *context, const Shape<D> &shape)
{
    context->Fill(shape.Flags & SandboxFlag_Fill);
    context->FillColor(shape.FillColor);

    context->Outline(shape.Flags & SandboxFlag_Outline);
    context->OutlineWidth(shape.OutlineWidth);
    context->OutlineColor(shape.OutlineColor);
}
template <Dimension D> static void drawShape(RenderContext<D> *context, const Shape<D> &shape)
{
    if (shape.Type.Geo == Geometry_Circle)
        context->Circle(shape.Transform.ComputeTransform(), shape.CircleOptions);
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
                ctx.Context->Axes(Meshes2.StaticMeshes[StaticMesh_Square].Mesh, {.Thickness = ctx.AxesThickness});
            else
                ctx.Context->Axes(Meshes3.StaticMeshes[StaticMesh_Cylinder].Mesh, {.Thickness = ctx.AxesThickness});
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
        //             ctx.Context->StaticMesh(Meshes3.StaticMeshes[StaticMesh_Sphere].Mesh);
        //             ctx.Context->Pop();
        //         }
        //         ctx.Context->PointLight(light);
        //     }
        // };
    }
}

template <Dimension D> Shape<D> SandboxAppLayer::CreateShape(const ContextData<D> &context)
{
    const Geometry geo = static_cast<Geometry>(context.GeometryToSpawn);
    Shape<D> shape{};
    shape.Type.Geo = geo;
    switch (geo)
    {
    case Geometry_Circle:
        shape.Name = "Circle";
        return shape;
    case Geometry_StaticMesh: {
        const Meshes<D> &meshes = GetMeshes<D>();
        const MeshId &mesh = meshes.StaticMeshes[context.StatMeshToSpawn];
        shape.Name = mesh.Name;
        shape.Mesh = mesh.Mesh;
        shape.Type.StaticMesh = context.StatMeshToSpawn;
        return shape;
    }
    default:
        TKIT_FATAL("[ONYX][SANDBOX] Unknown geometry");
        return shape;
    }
}

template <Dimension D> void SandboxAppLayer::AddContext(const Window *window)
{
    RenderContext<D> *context = VKIT_CHECK_EXPRESSION(Renderer::CreateContext<D>());
    auto &contexts = GetContexts<D>();
    ContextData<D> &data = contexts.Contexts.Append();
    data.Context = context;
    if constexpr (D == D3)
    {
        data.Flags = SandboxFlag_DrawAxes;
        data.DirLights.Append(context->AddDirectionalLight());
    }
    if (window)
        context->AddTarget(window);
}
SandboxWinLayer::SandboxWinLayer(ApplicationLayer *appLayer, Window *window, const Dimension dim)
    : WindowLayer(appLayer, window, {.Flags = WindowLayerFlag_ImGuiEnabled})
{
    SandboxAppLayer *alayer = GetApplicationLayer<SandboxAppLayer>();
    if (dim == D2)
    {
        AddCamera<D2>();
        alayer->AddContext<D2>(window);
    }
    else
    {
        AddCamera<D3>();
        alayer->AddContext<D3>(window);
    }
}

void SandboxWinLayer::OnRender(const DeltaTime &deltaTime)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnRender");
    if (!Cameras2.Cameras.IsEmpty())
    {
        Cameras2.Active = Math::Min(Cameras2.Cameras.GetSize() - 1, Cameras2.Active);
        Cameras2.Cameras[Cameras2.Active].Camera->ControlMovementWithUserInput(deltaTime.Measured);
    }
    if (!Cameras3.Cameras.IsEmpty())
    {
        Cameras3.Active = Math::Min(Cameras3.Cameras.GetSize() - 1, Cameras3.Active);
        Cameras3.Cameras[Cameras3.Active].Camera->ControlMovementWithUserInput(deltaTime.Measured);
    }

#ifdef ONYX_ENABLE_IMGUI
    RenderImGui();
#endif
}

void SandboxWinLayer::OnEvent(const Event &event)
{
    ProcessEvent<D2>(event);
    ProcessEvent<D3>(event);
}

#ifdef ONYX_ENABLE_IMGUI
static bool combo(const char *name, u32 *index, const char *items)
{
    i32 idx = static_cast<i32>(*index);
    if (ImGui::Combo(name, &idx, items))
    {
        *index = static_cast<u32>(idx);
        return true;
    }
    return false;
}
static bool combo(const char *name, u32 *index, const TKit::Span<const char *const> items)
{
    i32 idx = static_cast<i32>(*index);
    const i32 size = static_cast<i32>(items.GetSize());
    if (ImGui::Combo(name, &idx, items.GetData(), size))
    {
        *index = static_cast<u32>(idx);
        return true;
    }
    return false;
}

void SandboxWinLayer::RenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::RenderImGui");
    if (ImGuiDemoWindow)
        ImGui::ShowDemoWindow();
#    ifdef ONYX_ENABLE_IMPLOT
    if (ImPlotDemoWindow)
        ImPlot::ShowDemoWindow();
#    endif

    ApplicationLayer *appLayer = GetApplicationLayer();
    Window *window = GetWindow();
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
                RequestReloadImGui();
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
            "Onyx is a small application framework I have implemented to be used primarily in all projects I "
            "develop "
            "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
            "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input "
            "events. "
            "The framework is still in its early stages, but I plan to expand it further in the future.");

        ImGui::TextWrapped("This program is the Onyx demo, showcasing some of its features. Most of them can be tried "
                           "in the 'Editor' panel.");

        ImGui::TextLinkOpenURL("My GitHub", "https://github.com/ismawno");
        ImGui::Checkbox("Toggle ImGui demo window", &ImGuiDemoWindow);

        ImGui::TextWrapped(
            "You may load meshes for this demo to use for both 2D and 3D. Take into account that meshes may "
            "have been created with a different coordinate "
            "system or unit scaling values. In Onyx, shapes with unit transforms are supposed to be centered "
            "around "
            "zero with a cartesian coordinate system and size (from end to end) of one. That is why you may apply a "
            "transform before loading a specific mesh.");

        if (ImGui::CollapsingHeader("2D Meshes"))
            RenderMeshLoad<D2>();
        if (ImGui::CollapsingHeader("3D Meshes"))
            RenderMeshLoad<D3>();
    }
    ImGui::End();

    if (ImGui::Begin("Editor"))
    {
        ImGui::Text("This is the editor panel, where you can interact with the demo.");
        ImGui::TextWrapped(
            "Onyx windows can draw shapes in 2D and 3D, and have a separate API for each even though the "
            "window is shared. Users interact with the rendering API through rendering contexts.");
        PresentModeEditor(window, EditorFlag_DisplayHelp);

        const f32v2 spos = Input::GetScreenMousePosition(window);
        ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos[0], spos[1]);
        HelpMarkerSameLine("The screen mouse position is always Math::Normalized to the window size, always ranging "
                           "from -1 to 1 for 'x' and 'y', and from 0 to 1 for 'z'.");

        ImGui::BeginTabBar("Dimension");
        if (ImGui::BeginTabItem("2D"))
        {
            RenderContexts<D2>();
            RenderCameras<D2>();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("3D"))
        {
            RenderContexts<D3>();
            RenderCameras<D3>();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

template <Dimension D> void SandboxWinLayer::RenderCameras()
{
    if (ImGui::CollapsingHeader("Cameras"))
    {
        Window *window = GetWindow();
        auto &cameras = GetCameras<D>();
        if (cameras.Cameras.IsEmpty())
            ImGui::TextDisabled(
                "Window has no cameras for this dimension. At least one must be added to render anything 2D.");

        if (ImGui::Button("Add camera"))
            AddCamera<D>();

        renderSelectableNoTree(
            "Camera", cameras.Cameras, cameras.Active, [this](CameraData<D> &camera) { RenderCamera(camera); },
            [window](const CameraData<D> &camera) { window->DestroyCamera(camera.Camera); });
    }
}

template <Dimension D> void SandboxWinLayer::RenderCamera(CameraData<D> &camera)
{
    Camera<D> *cam = camera.Camera;
    const f32v2 vpos = cam->GetViewportMousePosition();
    ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos[0], vpos[1]);

    if constexpr (D == D2)
    {
        const f32v2 wpos2 = cam->GetWorldMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f)", wpos2[0], wpos2[1]);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &camera.ZOffset, 0.f, 1.f);
        HelpMarkerSameLine(
            "In 3D, the world mouse position can be ambiguous because of the extra dimension. This amibiguity needs to "
            "somehow be resolved. In most use-cases, ray casting is the best approach to fully define this position, "
            "but because this is a simple demo, the z offset can be manually specified, and is in the range [0, 1] "
            "(screen coordinates). Note that, if in perspective mode, 0 corresponds to the near plane and 1 to the "
            "far plane.");

        const f32v3 mpos3 = cam->GetWorldMousePosition(camera.ZOffset);
        const f32v2 vpos2 = cam->GetViewportMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f, %.2f)", mpos3[0], mpos3[1], mpos3[2]);
        ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos2[0], vpos2[1]);
    }
    HelpMarkerSameLine("The world mouse position has world units, meaning it takes into account the "
                       "transform of the camera to compute the mouse coordinates. It will not, however, "
                       "take into account the axes of any render context by default.");

    ImGui::Checkbox("Transparent", &cam->Transparent);
    if (!cam->Transparent)
        ImGui::ColorEdit3("Background", cam->BackgroundColor.GetData());

    ImGui::Text("Viewport");
    ImGui::SameLine();
    ScreenViewport viewport = cam->GetViewport();
    if (ViewportEditor(viewport, EditorFlag_DisplayHelp))
        cam->SetViewport(viewport);

    ImGui::Text("Scissor");
    ImGui::SameLine();
    ScreenScissor scissor = cam->GetScissor();
    if (ScissorEditor(scissor, EditorFlag_DisplayHelp))
        cam->SetScissor(scissor);

    const Transform<D> &view = cam->GetProjectionViewData().View;
    ImGui::Text("View transform");
    HelpMarkerSameLine(
        "The view transform are the coordinates of the camera, detached from any render context coordinate system.");

    DisplayTransform(view, EditorFlag_DisplayHelp);
    if constexpr (D == D3)
    {
        const f32v3 lookDir = cam->GetViewLookDirection();
        ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", lookDir[0], lookDir[1], lookDir[2]);
        HelpMarkerSameLine("The look direction is the direction the camera is facing. It is the "
                           "direction of the camera's 'forward' vector.");
    }
    if constexpr (D == D3)
    {
        i32 perspective = static_cast<i32>(camera.Perspective);
        if (ImGui::Combo("Projection", &perspective, "Orthographic\0Perspective\0\0"))
        {
            camera.Perspective = perspective == 1;
            if (camera.Perspective)
                cam->SetPerspectiveProjection(camera.FieldOfView, camera.Near, camera.Far);
            else
                cam->SetOrthographicProjection();
        }

        if (camera.Perspective)
        {
            f32 degs = Math::Degrees(camera.FieldOfView);

            bool changed = ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f);
            changed |= ImGui::SliderFloat("Near", &camera.Near, 0.1f, 10.f);
            changed |= ImGui::SliderFloat("Far", &camera.Far, 10.f, 100.f);
            if (changed)
            {
                camera.FieldOfView = Math::Radians(degs);
                cam->SetPerspectiveProjection(camera.FieldOfView, camera.Near, camera.Far);
            }
        }
    }

    ImGui::Text("The camera/view controls are the following:");
    DisplayCameraControls<D>();
    ImGui::TextWrapped(
        "The view describes the position and orientation of a camera in the scene. It is defined as a matrix "
        "that corresponds to the inverse of the camera's transform, and is applied to all objects in a context. "
        "When you 'move' a camera around, you are actually moving the scene (rendered by that camera) in the opposite "
        "direction. That is why the inverse is needed to transform the scene around you.");

    ImGui::TextWrapped("The projection is defined as an additional matrix that is applied on top of the view. It "
                       "projects and maps your scene onto your screen, and is responsible for the dimensions, "
                       "aspect ratio and, if using a 3D perspective, the field of view of the scene. In Onyx, only "
                       "orthographic and perspective projections are available. Orthographic projections are "
                       "embedded into the view's transform.");
    ImGui::TextWrapped("Orthographic projection: The scene is projected onto the screen without any perspective. "
                       "This means that objects do not get smaller as they move away from the camera. This is "
                       "useful for 2D games or when you want to keep the size of objects constant.");
    ImGui::TextWrapped("Perspective projection: The scene is projected onto the screen with perspective. This "
                       "means that objects get smaller as they move away from the camera, similar as how real life "
                       "vision behaves. This is useful for 3D games or when you want to create a sense of depth in "
                       "your scene. In Onyx, this projection is only available in 3D scenes.");
}

template <Dimension D> void SandboxWinLayer::RenderContexts()
{
    if (ImGui::CollapsingHeader("Contexts"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &contexts = appLayer->GetContexts<D>();

        if (ImGui::Button("Add context"))
            appLayer->AddContext<D>();

        HelpMarkerSameLine("A rendering context is an immediate mode API that allows users (you) to draw many "
                           "different objects in a window. Multiple contexts may exist per window, each with "
                           "their own independent state.");

        renderSelectableNoTree(
            "Context", contexts.Contexts, contexts.Active, [this](ContextData<D> &context) { RenderContext(context); },
            [](const ContextData<D> &context) { Renderer::DestroyContext(context.Context); });
    }
}

template <Dimension D> void SandboxWinLayer::RenderContext(ContextData<D> &context)
{
    const Window *window = GetWindow();
    const ViewMask viewBit = window->GetViewBit();
    bool targets = viewBit & context.Context->GetViewMask();
    if (ImGui::Checkbox("Target this window", &targets))
    {
        if (targets)
            context.Context->AddTarget(viewBit);
        else
            context.Context->RemoveTarget(viewBit);
    }

    ImGui::CheckboxFlags("Draw axes", &context.Flags, SandboxFlag_DrawAxes);
    if (context.Flags & SandboxFlag_DrawAxes)
        ImGui::SliderFloat("Axes thickness", &context.AxesThickness, 0.001f, 0.1f);

    ImGui::Spacing();
    ImGui::Text("Shape picker");
    RenderShapePicker<D>(context);
    ImGui::Spacing();
    ImGui::Text("Light picker");
    RenderLightPicker<D>(context);
}

template <Dimension D> void editShape(Shape<D> &shape)
{
    ImGui::PushID(&shape);
    ImGui::Text("Transform");
    ImGui::SameLine();
    TransformEditor<D>(shape.Transform, EditorFlag_DisplayHelp);
    ImGui::CheckboxFlags("Fill", &shape.Flags, SandboxFlag_Fill);
    ImGui::CheckboxFlags("Outline", &shape.Flags, SandboxFlag_Outline);
    ImGui::ColorEdit4("Fill color", shape.FillColor.GetData());
    ImGui::ColorEdit4("Outline color", shape.OutlineColor.GetData());
    ImGui::SliderFloat("Outline width", &shape.OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    if (shape.Type.Geo == Geometry_Circle)
    {
        ImGui::SliderFloat("Inner fade", &shape.CircleOptions.InnerFade, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Outer fade", &shape.CircleOptions.OuterFade, 0.f, 1.f, "%.2f");
        ImGui::SliderAngle("Lower angle", &shape.CircleOptions.LowerAngle);
        ImGui::SliderAngle("Upper angle", &shape.CircleOptions.UpperAngle);
        ImGui::SliderFloat("Hollowness", &shape.CircleOptions.Hollowness, 0.f, 1.f, "%.2f");
    }
    ImGui::PopID();
}

template <Dimension D> void SandboxWinLayer::RenderShapePicker(ContextData<D> &context)
{
    combo("Geometry", &context.GeometryToSpawn, "Circle\0Static mesh\0\0");
    const Geometry geo = static_cast<Geometry>(context.GeometryToSpawn);

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    TKit::StackArray<const char *> names{};

    if (geo == Geometry_StaticMesh)
    {
        const Meshes<D> &meshes = appLayer->GetMeshes<D>();
        names.Reserve(meshes.StaticMeshes.GetSize());
        for (const MeshId &mid : meshes.StaticMeshes)
            names.Append(mid.Name.c_str());
        combo("Shape", &context.StatMeshToSpawn, names);
    }

    if (ImGui::Button("Spawn##Shape"))
        context.Shapes.Append(appLayer->CreateShape<D>(context));

    renderSelectableNoRemoval(
        "Shapes", context.Shapes, context.SelectedShape, [](Shape<D> &shape) { editShape<D>(shape); },
        [](const Shape<D> &shape) { return shape.Name; });
}

template <Dimension D> void SandboxWinLayer::RenderLightPicker(ContextData<D> &context)
{
    Color ambient = context.Context->GetAmbientLight();
    if (ImGui::ColorEdit4("Ambient light", ambient.GetData()))
        context.Context->SetAmbientLight(ambient);

    if constexpr (D == D2)
        combo("Light type", &context.LightToSpawn, "Point\0\0");
    else
        combo("Light type", &context.LightToSpawn, "Point\0Directional\0\0");
    if (ImGui::Button("Spawn##Light"))
    {
        const LightType ltype = static_cast<LightType>(context.LightToSpawn);
        if (ltype == Light_Point)
            context.PointLights.Append(context.Context->AddPointLight());
        if constexpr (D == D3)
            if (ltype == Light_Directional)
                context.DirLights.Append(context.Context->AddDirectionalLight());
    }

    renderSelectable(
        "Point lights", context.PointLights, context.SelectedPointLight,
        [](PointLight<D> *light) { PointLightEditor(*light, EditorFlag_DisplayHelp); },
        [&context](PointLight<D> *light) { context.Context->RemovePointLight(light); }, "Point");
    if constexpr (D == D3)
        renderSelectable(
            "Directional lights", context.DirLights, context.SelectedDirLight,
            [](DirectionalLight *light) { DirectionalLightEditor(*light, EditorFlag_DisplayHelp); },
            [&context](DirectionalLight *light) { context.Context->RemoveDirectionalLight(light); }, "Directional");
}

template <Dimension D> void SandboxWinLayer::RenderMeshLoad()
{
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    Meshes<D> &meshes = appLayer->GetMeshes<D>();
    combo("Geometry", &meshes.GeoToLoad, "Static mesh\0\0");
    const Geometry geo = static_cast<Geometry>(meshes.GeoToLoad + 1); // skip circles
    if (geo == Geometry_StaticMesh)
    {
        const u32 importedIndex = D == D2 ? 2 : 4;
        if constexpr (D == D2)
            combo("Mesh", &meshes.StatMeshToLoad, "Regular polygon\0Polygon\0Imported\0\0");
        else
            combo("Mesh", &meshes.StatMeshToLoad, "Regular polygon\0Polygon\0Sphere\0Cylinder\0Imported\0\0");

        char name[256] = {0};
        ImGui::InputTextWithHint("Name", "Will default to mesh name", name, 256);
        if (meshes.StatMeshToLoad == 0)
        {
            const u32 mn = 3;
            const u32 mx = 128;
            ImGui::SliderScalar("Sides", ImGuiDataType_U32, &meshes.RegularPolySides, &mn, &mx);
            if (ImGui::Button("Load"))
            {
                appLayer->AddStaticMesh<D>(name[0] ? name : "Regular polygon",
                                           Assets::CreateRegularPolygonMesh<D>(meshes.RegularPolySides));
                VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
            }
        }
        else if (meshes.StatMeshToLoad == 1)
        {
            for (u32 i = 0; i < meshes.PolyVertices.GetSize();)
            {
                const f32v2 vx = meshes.PolyVertices[i];
                ImGui::PushID(&vx);
                if (ImGui::Button("X"))
                {
                    meshes.PolyVertices.RemoveOrdered(meshes.PolyVertices.begin() + i);
                    continue;
                }
                ImGui::SameLine();
                ImGui::BulletText("Vertex %u: (%f, %f)", i, vx[0], vx[1]);
                ImGui::PopID();

                ++i;
            }

            if (ImGui::Button("Add"))
                meshes.PolyVertices.Append(meshes.VertexToAdd);
            ImGui::SameLine();
            ImGui::DragFloat2("New vertex", meshes.VertexToAdd.GetData(), 0.4f);
            if (ImGui::Button("Load"))
            {
                appLayer->AddStaticMesh<D>(name[0] ? name : "Polygon",
                                           Assets::CreatePolygonMesh<D>(meshes.PolyVertices));
                VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
            }
        }
        else if (meshes.StatMeshToLoad == importedIndex)
        {
            if (ImGui::Button("Load"))
            {
                const auto load = [&](const Dialog::Path &path) {
                    const auto lres = Assets::LoadStaticMeshFromObj<D>(path.c_str());
                    VKIT_LOG_RESULT_ERROR(lres);
                    if (!lres)
                        return;

                    const StatMeshData<D> &data = lres.GetValue();
                    appLayer->AddStaticMesh(name[0] ? name : path.filename().c_str(), data);
                    VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
                };
                const char *path = D == D2 ? (ONYX_ROOT_PATH "/demo/meshes2/") : (ONYX_ROOT_PATH "/demo/meshes3/");
                const auto result = Dialog::OpenSingle({.Default = path});
                if (result)
                    load(result.GetValue());
            }
        }
        if constexpr (D == D3)
        {
            Meshes<D3> &m3 = meshes;
            if (meshes.StatMeshToLoad == 2)
            {
                const u32 mn = 8;
                const u32 mx = 256;
                ImGui::SliderScalar("Rings", ImGuiDataType_U32, &m3.Rings, &mn, &mx);
                ImGui::SliderScalar("Sectors", ImGuiDataType_U32, &m3.Sectors, &mn, &mx);
                if (ImGui::Button("Load"))
                {
                    appLayer->AddStaticMesh<D>(name[0] ? name : "Sphere",
                                               Assets::CreateSphereMesh(m3.Rings, m3.Sectors));
                    VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
                }
            }
            else if (meshes.StatMeshToLoad == 3)
            {
                const u32 mn = 8;
                const u32 mx = 256;
                ImGui::SliderScalar("Sides", ImGuiDataType_U32, &m3.CylinderSides, &mn, &mx);
                if (ImGui::Button("Load"))
                {
                    appLayer->AddStaticMesh<D>(name[0] ? name : "Cylinder",
                                               Assets::CreateCylinderMesh(m3.CylinderSides));
                    VKIT_CHECK_EXPRESSION(Assets::Upload<D>());
                }
            }
        }
    }
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
