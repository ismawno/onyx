#include "sandbox/sandbox.hpp"
#include "onyx/asset/assets.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/imgui.hpp"
#    include <misc/cpp/imgui_stdlib.h>
#endif
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/multiprocessing/topology.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "sandbox/argparse.hpp"

namespace Onyx
{
template <typename T> struct EntriesOptions
{
    const char *TreeName = nullptr;
    const char *EntryName = nullptr;
    u32 *Selected = nullptr;
    std::function<void(T &)> OnSelected = nullptr;
    std::function<void(T &)> OnRemoval = nullptr;
    std::function<const char *(const T &)> GetName = nullptr;
    u32 ScrollLimit = 6;
    bool RemoveButton = true;
};

template <typename C, typename T> static void renderEntries(C &entries, const EntriesOptions<T> &options)
{
    if (!entries.IsEmpty() && (!options.TreeName || ImGui::TreeNode(options.TreeName)))
    {
        const u32 count = entries.GetSize();
        ImGui::PushID(&entries);
        if (count > options.ScrollLimit)
            ImGui::BeginChild("##Scroll", ImVec2{0, 150});
        for (u32 i = 0; i < entries.GetSize(); ++i)
        {
            ImGui::PushID(&entries[i]);
            if (options.RemoveButton && ImGui::Button("X"))
            {
                if (options.OnRemoval)
                    options.OnRemoval(entries[i]);
                entries.RemoveOrdered(entries.begin() + i);
                ImGui::PopID();
                break;
            }
            if (options.RemoveButton)
                ImGui::SameLine();
            const char *name = options.GetName ? options.GetName(entries[i]) : options.EntryName;
            const std::string coolName = TKit::Format("{} - {}", i, name);
            if (options.Selected)
            {
                if (ImGui::Selectable(coolName.c_str(), i == *options.Selected))
                    *options.Selected = i;
            }
            else
                ImGui::BulletText("%s", coolName.c_str());

            ImGui::PopID();
        }
        if (count > options.ScrollLimit)
            ImGui::EndChild();
        if (options.Selected && *options.Selected < entries.GetSize())
            options.OnSelected(entries[*options.Selected]);
        ImGui::PopID();
        if (options.TreeName)
            ImGui::TreePop();
    }
}

SandboxAppLayer::SandboxAppLayer(const WindowLayers *layers, const ParseData *data) : ApplicationLayer(layers)
{
    AddMeshes<D2>();
    AddMeshes<D3>();

    AddSampler("Default sampler");

    MaterialPoolId<D2> &pool2 = AddMaterialPool<D2>("Default material pool 2D");
    MaterialPoolId<D3> &pool3 = AddMaterialPool<D3>("Default material pool 3D");

    AddMaterial<D2>(pool2, "Default material");
    AddMaterial<D3>(pool3, "Default material");

    ONYX_CHECK_EXPRESSION(Assets::Upload());

    if (data->Flags & ParseFlag_D2)
    {
        WindowSpecs wspecs{};
        wspecs.Title = "Onyx sandbox window (2D)";
        if (data->Flags & ParseFlag_AddLattice)
            RequestOpenWindow<SandboxWinLayer>(
                [this, data](const WindowLayer *, Window *window) { AddLattice(window, data->Lattice2); }, wspecs, D2);
        else
            RequestOpenWindow<SandboxWinLayer>(wspecs, D2);
    }

    if (data->Flags & ParseFlag_D3)
    {
        WindowSpecs wspecs{};
        wspecs.Title = "Onyx sandbox window (3D)";
        if (data->Flags & ParseFlag_AddLattice)
            RequestOpenWindow<SandboxWinLayer>(
                [this, data](const WindowLayer *, Window *window) { AddLattice(window, data->Lattice3); }, wspecs, D2);
        else
            RequestOpenWindow<SandboxWinLayer>(wspecs, D3);
    }
}
void SandboxAppLayer::OnTransfer(const DeltaTime &)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnTransfer");
    DrawShapes<D2>();
    DrawShapes<D3>();

    DrawLattices<D2>();
    DrawLattices<D3>();
}

template <Dimension D>
void SandboxAppLayer::AddStaticMesh(const char *name, const StatMeshData<D> &data, const bool upload)
{
    auto &meshes = GetMeshes<D>();
    const Mesh mesh = Assets::AddMesh(data);
    meshes.StaticMeshes.Append(name, mesh, data);
    if (upload)
    {
        ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
    }
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
}

template <Dimension D> static void setShapeProperties(RenderContext<D> *context, const Shape<D> &shape)
{
    context->Fill(shape.Flags & SandboxFlag_Fill);
    context->FillColor(shape.FillColor);

    context->Outline(shape.Flags & SandboxFlag_Outline);
    context->OutlineWidth(shape.OutlineWidth);
    context->OutlineColor(shape.OutlineColor);
    context->Material(shape.Material);
}
template <Dimension D> static void drawShape(RenderContext<D> *context, const Shape<D> &shape)
{
    if (shape.Type.Geo == Geometry_Circle)
        context->Circle(shape.Transform.ComputeTransform(), shape.CircleOptions);
    else
        context->StaticMesh(shape.StatMesh, shape.Transform.ComputeTransform());
}

template <Dimension D> void SandboxAppLayer::DrawShapes()
{
    const auto &contexts = GetContexts<D>();
    for (const ContextData<D> &ctx : contexts.Contexts)
        if (ctx.Flags & SandboxFlag_ContextShouldUpdate)
        {
            ctx.Context->Flush();
            ctx.Context->Push();
            for (const Shape<D> &shape : ctx.Shapes)
            {
                setShapeProperties(ctx.Context, shape);
                drawShape(ctx.Context, shape);
            }
            ctx.Context->Pop();
            ctx.Context->Push();
            for (const PointLight<D> *pl : ctx.PointLights)
            {
                ctx.Context->Scale(0.01f);
                ctx.Context->Translate(pl->GetPosition());
                ctx.Context->FillColor(pl->GetColor());
                if constexpr (D == D2)
                    ctx.Context->Circle();
                else
                    ctx.Context->StaticMesh(Meshes3.StaticMeshes[StaticMesh_Sphere].Mesh);
            }
            ctx.Context->Pop();

            if (ctx.Flags & SandboxFlag_DrawAxes)
            {
                ctx.Context->Outline(false);
                ctx.Context->Fill(true);
                ctx.Context->Material(ctx.AxesMaterial);
                if constexpr (D == D2)
                    ctx.Context->Axes(Meshes2.StaticMeshes[StaticMesh_Square].Mesh, {.Thickness = ctx.AxesThickness});
                else
                    ctx.Context->Axes(Meshes3.StaticMeshes[StaticMesh_Cylinder].Mesh, {.Thickness = ctx.AxesThickness});
            }
        }
}

template <Dimension D> void SandboxAppLayer::DrawLattices()
{
    const auto &lattices = GetLattices<D>();
    for (const LatticeData<D> &lattice : lattices.Lattices)
        if (lattice.Flags & SandboxFlag_ContextShouldUpdate)
        {
            const Geometry geo = Geometry(lattice.Shape.Type.Geo);
            switch (geo)
            {
            case Geometry_Circle: {
                const CircleOptions opts = lattice.Shape.CircleOptions;
                DrawLattice(lattice, [opts](const f32v<D> &pos, RenderContext<D> *context) {
                    context->SetTranslation(pos);
                    context->Circle(opts);
                });
                break;
            }
            case Geometry_StaticMesh: {
                const Mesh mesh = lattice.Shape.StatMesh;
                DrawLattice(lattice, [mesh](const f32v<D> &pos, RenderContext<D> *context) {
                    context->SetTranslation(pos);
                    context->StaticMesh(mesh);
                });
                break;
            }
            default:
                break;
            };
        }
}

template <Dimension D, typename F> void SandboxAppLayer::DrawLattice(const LatticeData<D> &lattice, F &&fun)
{
    TKit::ITaskManager *tm = Core::GetTaskManager();
    if constexpr (D == D2)
    {
        const u32 size = lattice.Dimensions[0] * lattice.Dimensions[1];
        const auto parallel = [&fun, &lattice](const u32 start, const u32 end) {
            TKIT_PROFILE_NSCOPE("Onyx::Sandbox::Lattice");

            const u32 tindex = TKit::Topology::GetThreadIndex();
            RenderContext<D> *context = lattice.Contexts[tindex];
            context->Flush();
            const f32v2 offset = lattice.Position - 0.5f * lattice.Separation * f32v2{lattice.Dimensions - 1u};

            setShapeProperties(context, lattice.Shape);
            context->Transform(lattice.Shape.Transform.ComputeTransform());
            for (u32 i = start; i < end; ++i)
            {
                const u32 ix = i / lattice.Dimensions[1];
                const u32 iy = i % lattice.Dimensions[1];
                const f32 x = lattice.Separation * f32(ix);
                const f32 y = lattice.Separation * f32(iy);

                const f32v2 pos = f32v2{x, y} + offset;
                std::forward<F>(fun)(pos, context);
            }
        };

        TKit::StackArray<Task> tasks{};
        tasks.Resize(lattice.Threads - 1);
        TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), lattice.Threads, parallel);
        for (u32 i = lattice.Threads; i < lattice.Contexts.GetSize(); ++i)
            lattice.Contexts[i]->Flush();

        for (u32 i = 0; i < lattice.Threads - 1; ++i)
            tm->WaitUntilFinished(tasks[i]);
    }
    else
    {
        const u32 size = lattice.Dimensions[0] * lattice.Dimensions[1] * lattice.Dimensions[2];
        const u32 yz = lattice.Dimensions[1] * lattice.Dimensions[2];
        const auto parallel = [yz, &fun, &lattice](const u32 start, const u32 end) {
            TKIT_PROFILE_NSCOPE("Onyx::Sandbox::Lattice");

            const u32 tindex = TKit::Topology::GetThreadIndex();
            RenderContext<D> *context = lattice.Contexts[tindex];
            context->Flush();
            const f32v3 offset = lattice.Position - 0.5f * lattice.Separation * f32v3{lattice.Dimensions - 1u};

            setShapeProperties(context, lattice.Shape);
            context->Transform(lattice.Shape.Transform.ComputeTransform());
            for (u32 i = start; i < end; ++i)
            {
                const u32 ix = i / yz;
                const u32 j = ix * yz;
                const u32 iy = (i - j) / lattice.Dimensions[2];
                const u32 iz = (i - j) % lattice.Dimensions[2];
                const f32 x = lattice.Separation * f32(ix);
                const f32 y = lattice.Separation * f32(iy);
                const f32 z = lattice.Separation * f32(iz);
                const f32v3 pos = f32v3{x, y, z} + offset;
                std::forward<F>(fun)(pos, context);
            }
        };

        TKit::StackArray<Task> tasks{};
        tasks.Resize(lattice.Threads - 1);
        TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), lattice.Threads, parallel);

        for (u32 i = lattice.Threads; i < lattice.Contexts.GetSize(); ++i)
            lattice.Contexts[i]->Flush();

        for (u32 i = 0; i < lattice.Threads - 1; ++i)
            tm->WaitUntilFinished(tasks[i]);
    }
}

template <Dimension D> Shape<D> SandboxAppLayer::CreateShape(const u32 geometry, const u32 statMesh)
{
    const Geometry geo = Geometry(geometry);
    Shape<D> shape{};
    shape.Type.Geo = geo;
    shape.Material = 0; // assuming at least a material is automatically added
    switch (geo)
    {
    case Geometry_Circle:
        shape.Name = "Circle";
        return shape;
    case Geometry_StaticMesh: {
        const auto &meshes = GetMeshes<D>();
        const StatMeshId<D> &mesh = meshes.StaticMeshes[statMesh];
        shape.Name = mesh.Name;
        shape.StatMesh = mesh.Mesh;
        shape.Type.StaticMesh = statMesh;
        return shape;
    }
    default:
        TKIT_FATAL("[ONYX][SANDBOX] Unknown geometry");
        return shape;
    }
}

template <Dimension D> void SandboxAppLayer::AddContext(const Window *window)
{
    RenderContext<D> *context = ONYX_CHECK_EXPRESSION(Renderer::CreateContext<D>());
    auto &contexts = GetContexts<D>();
    ContextData<D> &data = contexts.Contexts.Append();
    data.Context = context;
    data.AxesMaterial = 0;
    if constexpr (D == D3)
    {
        data.Flags |= SandboxFlag_DrawAxes;
        data.DirLights.Append(context->AddDirectionalLight());
    }
    if (window)
        context->AddTarget(window);
}

template <Dimension D> void SandboxAppLayer::AddLattice(const Window *window, const LatticeData<D> &lattice)
{
    auto &lattices = GetLattices<D>();
    LatticeData<D> &data = lattices.Lattices.Append(lattice);
    for (u32 i = 0; i < TKit::MaxThreads; ++i)
    {
        RenderContext<D> *ctx = ONYX_CHECK_EXPRESSION(Renderer::CreateContext<D>());
        if (window)
            ctx->AddTarget(window);
        data.Contexts[i] = ctx;
    }
    data.Shape = CreateShape(data);
}

template <Dimension D> MaterialPoolId<D> &SandboxAppLayer::AddMaterialPool(const char *name)
{
    auto &materials = GetMaterials<D>();
    MaterialPoolId<D> &pool = materials.Pools.Append();
    pool.Pool = ONYX_CHECK_EXPRESSION(Assets::CreateMaterialPool<D>());
    pool.Name = name ? name : TKit::Format("Material-pool-{}", pool.Pool);
    return pool;
}

template <Dimension D> void SandboxAppLayer::AddMaterial(MaterialPoolId<D> &pool, const char *name)
{
    auto &materials = pool.Data;
    MaterialId<D> &mat = materials.Materials.Append();
    mat.Material = Assets::AddMaterial(pool.Pool, mat.Data);
    mat.Name = name ? name : TKit::Format("Material-{}", mat.Material);
    ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
}

void SandboxAppLayer::AddSampler(const char *name)
{
    SamplerId &sampler = Samplers.Append();
    sampler.Sampler = Assets::AddSampler(sampler.Data);
    sampler.Name = name ? name : TKit::Format("Sampler-{}", sampler.Sampler);
    ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
}

void SandboxAppLayer::AddTexture(const TextureData &data, const char *name)
{
    TextureId &tex = Textures.Append();
    tex.Texture = Assets::AddTexture(data);
    tex.Name = name ? name : TKit::Format("Texture-{}", tex.Texture);
    ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
}

template <Dimension D> void SandboxAppLayer::UpdateMaterialData()
{
    auto &materials = GetMaterials<D>();
    for (MaterialPoolId<D> &pool : materials.Pools)
        for (MaterialId<D> &mat : pool.Data.Materials)
            mat.Data = Assets::GetMaterialData<D>(mat.Material);
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
SandboxWinLayer::~SandboxWinLayer()
{
    const auto wait = [](TKit::Task<Dialog::Result<Dialog::Path>> &task) {
        if (task)
        {
            TKit::ITaskManager *tm = Core::GetTaskManager();
            tm->WaitUntilFinished(task);
        }
    };
    wait(StatMeshTask);
    wait(TexTask);
    wait(GltfTask);
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
    i32 idx = i32(*index);
    if (ImGui::Combo(name, &idx, items))
    {
        *index = u32(idx);
        return true;
    }
    return false;
}
static bool combo(const char *name, u32 *index, const TKit::Span<const char *const> items)
{
    i32 idx = i32(*index);
    const i32 size = i32(items.GetSize());
    if (ImGui::Combo(name, &idx, items.GetData(), size))
    {
        *index = u32(idx);
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
        DeltaTimeEditor(EditorFlag_DisplayHelp);
        if (ImGui::TreeNode("Update delta time"))
        {
            appLayer->UpdateDeltaTimeEditor(EditorFlag_DisplayHelp);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Transfer delta time"))
        {
            appLayer->TransferDeltaTimeEditor(EditorFlag_DisplayHelp);
            ImGui::TreePop();
        }
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
#    ifdef ONYX_ENABLE_IMPLOT
        ImGui::Checkbox("Toggle ImPlot demo window", &ImPlotDemoWindow);
#    endif
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
            RenderMeshes<D2>();
            RenderMaterialPools<D2>();
            RenderSamplers();
            RenderTextures();
            RenderLattices<D2>();
            RenderRenderer<D2>();
            RenderGltf<D2>();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("3D"))
        {
            RenderContexts<D3>();
            RenderCameras<D3>();
            RenderMeshes<D3>();
            RenderMaterialPools<D3>();
            RenderSamplers();
            RenderTextures();
            RenderLattices<D3>();
            RenderRenderer<D3>();
            RenderGltf<D3>();
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

        EntriesOptions<CameraData<D>> opts{};
        opts.EntryName = "Camera";
        opts.Selected = &cameras.Active;
        opts.OnSelected = [this](CameraData<D> &camera) { RenderCamera(camera); };
        opts.OnRemoval = [window](const CameraData<D> &camera) { window->DestroyCamera(camera.Camera); };
        renderEntries(cameras.Cameras, opts);
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
        i32 perspective = i32(camera.Perspective);
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

        EntriesOptions<ContextData<D>> opts{};
        opts.EntryName = "Context";
        opts.Selected = &contexts.Active;
        opts.OnSelected = [this](ContextData<D> &context) { RenderContext(context); };
        opts.OnRemoval = [](const ContextData<D> &context) { Renderer::DestroyContext(context.Context); };
        renderEntries(contexts.Contexts, opts);
    }
}

template <Dimension D> static bool matNameCombo(const char *name, SandboxAppLayer *appLayer, Material *material)
{
    ImGui::PushID(material);
    TKit::StackArray<const char *> names{};
    const auto &materials = appLayer->GetMaterials<D>();

    u32 size = 0;
    for (const MaterialPoolId<D> &pool : materials.Pools)
        size += pool.Data.Materials.GetSize();

    names.Reserve(size);

    u32 idx = TKIT_U32_MAX;
    u32 gidx = 0;
    for (const MaterialPoolId<D> &pool : materials.Pools)
        for (const MaterialId<D> &mat : pool.Data.Materials)
        {
            names.Append(mat.Name.c_str());
            if (mat.Material == *material)
                idx = gidx;
            ++gidx;
        }

    bool changed = false;
    if (*material != NullMaterial)
    {
        changed |= ImGui::Button("X");
        if (changed)
        {
            *material = NullMaterial;
            idx = TKIT_U32_MAX;
        }
        ImGui::SameLine();
    }
    ImGui::PopID();
    if (combo(name, &idx, names))
    {
        for (const MaterialPoolId<D> &pool : materials.Pools)
            if (idx < pool.Data.Materials.GetSize())
            {
                *material = pool.Data.Materials[idx].Material;
                break;
            }
            else
                idx -= pool.Data.Materials.GetSize();

        changed = true;
    }
    return changed;
}

template <typename T> static bool nameCombo(const char *name, const TKit::TierArray<T> &container, Asset *handle)
{
    ImGui::PushID(handle);
    TKit::StackArray<const char *> names{};
    names.Reserve(container.GetSize());
    u32 idx = TKIT_U32_MAX;
    for (u32 i = 0; i < container.GetSize(); ++i)
    {
        const T &elm = container[i];
        names.Append(elm.Name.c_str());
        if constexpr (std::is_same_v<T, TextureId>)
        {
            if (elm.Texture == *handle)
                idx = i;
        }
        else
        {
            if (elm.Sampler == *handle)
                idx = i;
        }
    }
    bool changed = false;
    if (*handle != NullAsset)
    {
        changed |= ImGui::Button("X");
        if (changed)
        {
            *handle = NullAsset;
            idx = TKIT_U32_MAX;
        }
        ImGui::SameLine();
    }
    ImGui::PopID();
    if (combo(name, &idx, names))
    {
        if constexpr (std::is_same_v<T, TextureId>)
            *handle = container[idx].Texture;
        else
            *handle = container[idx].Sampler;
        changed = true;
    }
    return changed;
}

static bool texNameCombo(const char *name, SandboxAppLayer *appLayer, Texture *toSpawn)
{
    return nameCombo(name, appLayer->Textures, toSpawn);
}
static bool sampNameCombo(const char *name, SandboxAppLayer *appLayer, Sampler *toSpawn)
{
    return nameCombo(name, appLayer->Samplers, toSpawn);
}

template <Dimension D> void SandboxWinLayer::RenderContext(ContextData<D> &context)
{
    const Window *window = GetWindow();
    const ViewMask viewBit = window->GetViewBit();

    ImGui::CheckboxFlags("Continuous update##Context", &context.Flags, SandboxFlag_ContextShouldUpdate);
    bool targets = viewBit & context.Context->GetViewMask();
    if (ImGui::Checkbox("Target this window##Context", &targets))
    {
        if (targets)
            context.Context->AddTarget(viewBit);
        else
            context.Context->RemoveTarget(viewBit);
    }

    ImGui::CheckboxFlags("Draw axes", &context.Flags, SandboxFlag_DrawAxes);
    if (context.Flags & SandboxFlag_DrawAxes)
    {
        ImGui::SliderFloat("Axes thickness", &context.AxesThickness, 0.001f, 0.1f);
        matNameCombo<D>("Axes material", GetApplicationLayer<SandboxAppLayer>(), &context.AxesMaterial);
    }

    ImGui::Spacing();
    ImGui::Text("Shape picker");
    RenderShapePicker<D>(context);
    ImGui::Spacing();
    ImGui::Text("Light picker");
    RenderLightPicker<D>(context);
}

template <Dimension D> static void editShape(Shape<D> &shape, SandboxAppLayer *appLayer)
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

    matNameCombo<D>("Material##Shape", appLayer, &shape.Material);

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

template <Dimension D> static bool shapeNameCombo(const char *name, SandboxAppLayer *appLayer, u32 *toSpawn)
{
    TKit::StackArray<const char *> names{};
    const auto &meshes = appLayer->GetMeshes<D>();
    names.Reserve(meshes.StaticMeshes.GetSize());
    for (const StatMeshId<D> &mid : meshes.StaticMeshes)
        names.Append(mid.Name.c_str());
    return combo(name, toSpawn, names);
}

template <Dimension D> void SandboxWinLayer::RenderShapePicker(ContextData<D> &context)
{
    combo("Geometry##Picker", &context.GeometryToSpawn, "Circle\0Static mesh\0\0");
    const Geometry geo = Geometry(context.GeometryToSpawn);

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();

    if (geo == Geometry_StaticMesh)
        shapeNameCombo<D>("Shape##Picker", appLayer, &context.StatMeshToSpawn);

    if (ImGui::Button("Spawn##Shape"))
        context.Shapes.Append(appLayer->CreateShape<D>(context));

    EntriesOptions<Shape<D>> opts{};
    opts.TreeName = "Shapes";
    opts.Selected = &context.SelectedShape;
    opts.OnSelected = [appLayer](Shape<D> &shape) { editShape<D>(shape, appLayer); };
    opts.GetName = [](const Shape<D> &shape) { return shape.Name.c_str(); };
    renderEntries(context.Shapes, opts);
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
        const LightType ltype = LightType(context.LightToSpawn);
        if (ltype == Light_Point)
            context.PointLights.Append(context.Context->AddPointLight());
        if constexpr (D == D3)
            if (ltype == Light_Directional)
                context.DirLights.Append(context.Context->AddDirectionalLight());
    }

    EntriesOptions<PointLight<D> *> popts{};
    popts.TreeName = "Point lights";
    popts.EntryName = "Point light";
    popts.Selected = &context.SelectedPointLight;
    popts.OnSelected = [](PointLight<D> *light) { PointLightEditor(*light, EditorFlag_DisplayHelp); };
    popts.OnRemoval = [&context](PointLight<D> *light) { context.Context->RemovePointLight(light); };
    renderEntries(context.PointLights, popts);

    if constexpr (D == D3)
    {
        EntriesOptions<DirectionalLight *> dopts{};
        dopts.TreeName = "Directional lights";
        dopts.EntryName = "Directional light";
        dopts.Selected = &context.SelectedDirLight;
        dopts.OnSelected = [](DirectionalLight *light) { DirectionalLightEditor(*light, EditorFlag_DisplayHelp); };
        dopts.OnRemoval = [&context](DirectionalLight *light) { context.Context->RemoveDirectionalLight(light); };
        renderEntries(context.DirLights, dopts);
    }
}

template <Dimension D> void SandboxWinLayer::RenderLattices()
{
    if (ImGui::CollapsingHeader("Performance"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &lattices = appLayer->GetLattices<D>();

        if (ImGui::Button("Add lattice"))
            appLayer->AddLattice<D>(GetWindow());

        EntriesOptions<LatticeData<D>> opts{};
        opts.EntryName = "Lattice";
        opts.Selected = &lattices.Active;
        opts.OnSelected = [this](LatticeData<D> &lattice) { RenderLattice(lattice); };
        opts.OnRemoval = [](const LatticeData<D> &lattice) {
            for (Onyx::RenderContext<D> *ctx : lattice.Contexts)
                Renderer::DestroyContext(ctx);
        };
        renderEntries(lattices.Lattices, opts);
    }
}
template <Dimension D> void SandboxWinLayer::RenderMaterialPools()
{
    if (ImGui::CollapsingHeader("Materials"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &materials = appLayer->GetMaterials<D>();
        if (ImGui::Button("Add pool"))
            appLayer->AddMaterialPool<D>();

        EntriesOptions<MaterialPoolId<D>> opts{};
        opts.Selected = &materials.Active;
        opts.OnSelected = [this](MaterialPoolId<D> &pool) { RenderMaterialPool(pool); };
        opts.GetName = [](const MaterialPoolId<D> &pool) { return pool.Name.c_str(); };
        opts.OnRemoval = [appLayer](MaterialPoolId<D> &pool) {
            Assets::DestroyMaterialPool<D>(pool.Pool);

            auto &contexts = appLayer->GetContexts<D>();
            for (ContextData<D> &ctx : contexts.Contexts)
            {
                for (Shape<D> &shape : ctx.Shapes)
                    if (Assets::GetPoolHandle(shape.Material) == pool.Pool)
                        shape.Material = NullMaterial;
                if (Assets::GetPoolHandle(ctx.AxesMaterial) == pool.Pool)
                    ctx.AxesMaterial = NullMaterial;
            }
            ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
        };
        renderEntries(materials.Pools, opts);
    }
}
template <Dimension D> void SandboxWinLayer::RenderMaterialPool(MaterialPoolId<D> &pool)
{
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    auto &materials = pool.Data;

    if (ImGui::Button("Add material"))
        appLayer->AddMaterial<D>(pool);

    EntriesOptions<MaterialId<D>> opts{};
    opts.Selected = &materials.Active;
    opts.OnSelected = [this](MaterialId<D> &material) { RenderMaterial(material); };
    opts.GetName = [](const MaterialId<D> &material) { return material.Name.c_str(); };
    opts.RemoveButton = false;
    renderEntries(materials.Materials, opts);
}

template <Dimension D> void SandboxWinLayer::RenderMaterial(MaterialId<D> &material)
{
    ImGui::InputText("Name##Material", &material.Name);
    ImGui::Text("Material id: %u", material.Material);
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    bool changed = MaterialPropertiesEditor(material.Data, EditorFlag_DisplayHelp);
    if constexpr (D == D2)
    {
        changed |= sampNameCombo("Sampler", appLayer, &material.Data.Sampler);
        changed |= texNameCombo("Texture", appLayer, &material.Data.Texture);
    }
    else
    {
        constexpr TKit::FixedArray<const char *, TextureSlot_Count> texNames = {
            "Albedo tex", "Metallic roughness tex", "Normal tex", "Occlusion tex", "Emissive tex"};
        constexpr TKit::FixedArray<const char *, TextureSlot_Count> sampNames = {
            "Albedo sampler", "Metallic roughness sampler", "Normal sampler", "Occlusion sampler", "Emissive sampler"};
        for (u32 i = 0; i < TextureSlot_Count; ++i)
        {
            changed |= texNameCombo(texNames[i], appLayer, &material.Data.Textures[i]);
            changed |= sampNameCombo(sampNames[i], appLayer, &material.Data.Samplers[i]);
            ImGui::Spacing();
        }
    }
    if (changed)
    {
        Assets::UpdateMaterial(material.Material, material.Data);
        ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
    }
}

template <typename F>
void SandboxWinLayer::HandleLoadDialog([[maybe_unused]] TKit::Task<Dialog::Result<Dialog::Path>> &task, const F load,
                                       const char *name)
{
    const auto handleError = [](const Dialog::Status status) {
        switch (status)
        {
        case Dialog::Success:
            return;
        case Dialog::Cancel:
            TKIT_LOG_WARNING("[ONYX][SANDBOX] Selection canceled");
            return;
        case Dialog::Error: {
            const char *error = Dialog::GetError();
            TKIT_LOG_ERROR_IF(error, "[ONYX][SANDBOX] Error opening dialog: {}", error);
            TKIT_LOG_ERROR_IF(!error, "[ONYX][SANDBOX] Error opening dialog");
            if (error)
                Dialog::ClearError();
            return;
        }
        }
    };
#    ifndef TKIT_OS_APPLE
    ImGui::BeginDisabled(task && !task.IsFinished());
    TKit::ITaskManager *tm = Core::GetTaskManager();
#    endif
    ImGui::PushID(&load);
    if (ImGui::Button(name))
    {
        const auto openDialog = [this]() { return Dialog::OpenSingle({.Window = GetWindow()->GetHandle()}); };
#    ifndef TKIT_OS_APPLE
        task.Reset();
        task = openDialog;
        tm->SubmitTask(&task);
#    else
        const auto result = openDialog();
        if (result)
            load(result.GetValue());
        else
            handleError(result.GetError());
#    endif
    }
#    ifndef TKIT_OS_APPLE
    ImGui::EndDisabled();
    if (task && task.IsFinished())
    {
        const auto result = tm->WaitForResult(task);
        if (result)
            load(result.GetValue());
        else
            handleError(result.GetError());
        task = nullptr;
    }
#    endif
    ImGui::PopID();
}

void SandboxWinLayer::RenderSamplers()
{
    if (ImGui::CollapsingHeader("Samplers"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        if (ImGui::Button("Create"))
            appLayer->AddSampler();

        EntriesOptions<SamplerId> opts{};
        opts.GetName = [](const SamplerId &samp) { return samp.Name.c_str(); };
        opts.OnRemoval = [appLayer](SamplerId &samp) {
            Assets::RemoveSampler(samp.Sampler);
            appLayer->UpdateMaterialData<D2>();
            appLayer->UpdateMaterialData<D3>();
            ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
        };
        opts.OnSelected = [this](SamplerId &samp) { RenderSampler(samp); };
        opts.Selected = &appLayer->SelectedSampler;
        renderEntries(appLayer->Samplers, opts);
    }
}

void SandboxWinLayer::RenderSampler(SamplerId &sampler)
{
    ImGui::InputText("Name##Sampler", &sampler.Name);
    ImGui::Text("Sampler id: %u", sampler.Sampler);
    if (SamplerEditor(sampler.Data, EditorFlag_DisplayHelp))
    {
        Assets::UpdateSampler(sampler.Sampler, sampler.Data);
        ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
    }
}

void SandboxWinLayer::RenderTextures()
{
    if (ImGui::CollapsingHeader("Textures"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        HandleLoadDialog(TexTask, [&](const Dialog::Path &path) {
            const auto res = Assets::LoadTextureDataFromImageFile(path.string().c_str(), ImageComponent_RGBA);
            VKIT_LOG_RESULT_ERROR(res);
            if (!res)
                return;

            const TextureData &data = res.GetValue();
            appLayer->AddTexture(data, path.filename().string().c_str());
            ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
        });

        EntriesOptions<TextureId> opts{};
        opts.GetName = [](const TextureId &tex) { return tex.Name.c_str(); };
        opts.OnRemoval = [appLayer](TextureId &tex) {
            Assets::RemoveTexture(tex.Texture);
            appLayer->UpdateMaterialData<D2>();
            appLayer->UpdateMaterialData<D3>();
            ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
        };
        renderEntries(appLayer->Textures, opts);
    }
}

template <Dimension D> void SandboxWinLayer::RenderGltf()
{
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    HandleLoadDialog(
        GltfTask,
        [&](const Dialog::Path &path) {
            auto res = Assets::LoadGltfAssetsFromFile<D>(
                path.string(), LoadGltfDataFlag_ForceRGBA | LoadGltfDataFlag_CenterVerticesAroundOrigin);
            VKIT_LOG_RESULT_ERROR(res);
            if (!res)
                return;

            MaterialPoolId<D> &pool = appLayer->AddMaterialPool<D>();

            GltfAssets<D> &assets = res.GetValue();
            const GltfHandles handles = Assets::AddGltfAssets(pool.Pool, assets);
            ONYX_CHECK_EXPRESSION(Assets::RequestUpload());

            auto &meshes = appLayer->GetMeshes<D>();
            for (u32 i = 0; i < handles.StaticMeshes.GetSize(); ++i)
            {
                const Mesh mesh = handles.StaticMeshes[i];
                const StatMeshData<D> &mdat = assets.StaticMeshes[i];
                StatMeshId<D> &mid = meshes.StaticMeshes.Append();
                mid.Name = TKit::Format("GLTF-Mesh-{}", mesh);
                mid.Mesh = mesh;
                mid.Data = mdat;
            }

            for (u32 i = 0; i < handles.Materials.GetSize(); ++i)
            {
                const Material mat = handles.Materials[i];
                const MaterialData<D> &mdat = assets.Materials[i];
                MaterialId<D> &mid = pool.Data.Materials.Append();
                mid.Name = TKit::Format("GLTF-Material-{}", mat);
                mid.Material = mat;
                mid.Data = mdat;
            }

            auto &samplers = appLayer->Samplers;
            for (u32 i = 0; i < handles.Samplers.GetSize(); ++i)
            {
                const Sampler samp = handles.Samplers[i];
                const SamplerData &sdata = assets.Samplers[i];
                SamplerId &sid = samplers.Append();
                sid.Name = TKit::Format("GLTF-Sampler-{}", samp);
                sid.Sampler = samp;
                sid.Data = sdata;
            }

            auto &textures = appLayer->Textures;
            for (const Texture tex : handles.Textures)
            {
                TextureId &tid = textures.Append();
                tid.Name = TKit::Format("GLTF-Texture-{}", tex);
                tid.Texture = tex;
            }
        },
        "Load GLTF file");
}

template <Dimension D> void SandboxWinLayer::RenderMeshes()
{
    if (ImGui::CollapsingHeader("Meshes"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &meshes = appLayer->GetMeshes<D>();

        EntriesOptions<StatMeshId<D>> opts{};
        opts.TreeName = "Static meshes";
        opts.Selected = &meshes.Active;
        opts.OnSelected = [this](StatMeshId<D> &mesh) { RenderMesh(mesh); };
        opts.GetName = [](const StatMeshId<D> &mesh) { return mesh.Name.c_str(); };
        opts.RemoveButton = false;
        renderEntries(meshes.StaticMeshes, opts);

        combo("Geometry##Load", &meshes.GeometryToLoad, "Static mesh\0\0");
        const Geometry geo = Geometry(meshes.GeometryToLoad + 1); // skip circles
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
                if (ImGui::Button("Create##Regular"))
                    appLayer->AddStaticMesh<D>(name[0] ? name : "Regular polygon",
                                               Assets::CreateRegularPolygonMesh<D>(meshes.RegularPolySides), true);
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
                if (ImGui::Button("Create##Polygon"))
                    appLayer->AddStaticMesh<D>(name[0] ? name : "Polygon",
                                               Assets::CreatePolygonMesh<D>(meshes.PolyVertices));
            }
            else if (meshes.StatMeshToLoad == importedIndex)
                HandleLoadDialog(StatMeshTask, [&](const Dialog::Path &path) {
                    const auto res = Assets::LoadStaticMeshFromObjFile<D>(path.string().c_str(),
                                                                          LoadObjDataFlag_CenterVerticesAroundOrigin);
                    VKIT_LOG_RESULT_ERROR(res);
                    if (!res)
                        return;

                    const StatMeshData<D> &data = res.GetValue();
                    appLayer->AddStaticMesh(name[0] ? name : path.filename().string().c_str(), data, true);
                });
            if constexpr (D == D3)
            {
                MeshArray<D3> &m3 = meshes;
                if (meshes.StatMeshToLoad == 2)
                {
                    const u32 mn = 8;
                    const u32 mx = 256;
                    ImGui::SliderScalar("Rings", ImGuiDataType_U32, &m3.Rings, &mn, &mx);
                    ImGui::SliderScalar("Sectors", ImGuiDataType_U32, &m3.Sectors, &mn, &mx);
                    if (ImGui::Button("Create##Sphere"))
                        appLayer->AddStaticMesh<D>(name[0] ? name : "Sphere",
                                                   Assets::CreateSphereMesh(m3.Rings, m3.Sectors), true);
                }
                else if (meshes.StatMeshToLoad == 3)
                {
                    const u32 mn = 8;
                    const u32 mx = 256;
                    ImGui::SliderScalar("Sides", ImGuiDataType_U32, &m3.CylinderSides, &mn, &mx);
                    if (ImGui::Button("Create##Cylinder"))
                        appLayer->AddStaticMesh<D>(name[0] ? name : "Cylinder",
                                                   Assets::CreateCylinderMesh(m3.CylinderSides), true);
                }
            }
        }
        ImGui::TextWrapped(
            "You may load meshes for this demo to use for both 2D and 3D. Take into account that meshes may "
            "have been created with a different coordinate "
            "system or unit scaling values. In Onyx, shapes with unit transforms are supposed to be centered "
            "around "
            "zero with a cartesian coordinate system and size (from end to end) of one. That is why you may apply a "
            "transform before loading a specific mesh.");
    }
}

template <Dimension D> void SandboxWinLayer::RenderMesh(StatMeshId<D> &mesh)
{
    bool changed = false;
    if (ImGui::TreeNode("Vertex buffer"))
    {
        for (u32 i = 0; i < mesh.Data.Vertices.GetSize(); ++i)
        {
            StatVertex<D> &vx = mesh.Data.Vertices[i];
            ImGui::Text("Vertex %u: ", i);
            ImGui::PushID(i);
            if constexpr (D == D2)
            {
                ImGui::DragFloat2("Position", vx.Position.GetData(), 0.1f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat2("UV", vx.TexCoord.GetData(), 0.05f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
            }
            else
            {
                ImGui::DragFloat3("Position", vx.Position.GetData(), 0.1f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat2("UV", vx.TexCoord.GetData(), 0.05f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat3("Normal", vx.Normal.GetData(), 0.1f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat4("Tangent", vx.Tangent.GetData(), 0.1f);
                changed |= ImGui::IsItemDeactivatedAfterEdit();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Index buffer"))
    {
        for (u32 i = 0; i < mesh.Data.Indices.GetSize(); ++i)
        {
            Index &idx = mesh.Data.Indices[i];
            ImGui::Text("Index %u: ", i);
            ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::DragScalar("##Index", ImGuiDataType_U32, &idx);
            changed |= ImGui::IsItemDeactivatedAfterEdit();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if (changed)
    {
        Assets::UpdateMesh(mesh.Mesh, mesh.Data);
        ONYX_CHECK_EXPRESSION(Assets::RequestUpload());
    }
}

template <Dimension D> void SandboxWinLayer::RenderRenderer()
{
    if (ImGui::CollapsingHeader("Renderer"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        bool coalesce = appLayer->GetCoalescePeriod() != 0;
        if (ImGui::Checkbox("Coalesce##Checkbox", &coalesce))
        {
            if (coalesce)
                appLayer->SetCoalescePeriod(1);
            else
                appLayer->SetCoalescePeriod(0);
        }
        if (coalesce)
        {
            u32 freq = appLayer->GetCoalescePeriod();
            u32 mn = 1;
            u32 mx = 128;
            ImGui::SliderScalar("Coalesce period", ImGuiDataType_U32, &freq, &mn, &mx);
        }
        Renderer::DisplayMemoryLayout<D>();
    }
}

template <Dimension D> void SandboxWinLayer::RenderLattice(LatticeData<D> &lattice)
{
    const Window *window = GetWindow();
    const ViewMask viewBit = window->GetViewBit();

    ImGui::CheckboxFlags("Continuous update##Lattice", &lattice.Flags, SandboxFlag_ContextShouldUpdate);

    bool targets = viewBit & lattice.Contexts[0]->GetViewMask();
    if (ImGui::Checkbox("Target this window##Lattice", &targets))
    {
        if (targets)
            for (Onyx::RenderContext<D> *ctx : lattice.Contexts)
                ctx->AddTarget(viewBit);
        else
            for (Onyx::RenderContext<D> *ctx : lattice.Contexts)
                ctx->RemoveTarget(viewBit);
    }
    bool updateShape = combo("Geometry##Lattice", &lattice.Geometry, "Circle\0Static mesh\0\0");

    const Geometry geo = Geometry(lattice.Geometry);

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    if (geo == Geometry_StaticMesh)
        updateShape |= shapeNameCombo<D>("Shape##Lattice", appLayer, &lattice.StatMesh);

    if (updateShape)
        lattice.Shape = appLayer->CreateShape(lattice);

    if constexpr (D == D2)
    {
        ImGui::DragFloat2("Position", lattice.Position.GetData(), 0.1f);
        ImGui::DragScalarN("Dimensions", ImGuiDataType_U32, lattice.Dimensions.GetData(), 2, 0.1f);
    }
    else
    {
        ImGui::DragFloat3("Position", lattice.Position.GetData(), 0.1f);
        ImGui::DragScalarN("Dimensions", ImGuiDataType_U32, lattice.Dimensions.GetData(), 3, 0.1f);
    }
    ImGui::DragFloat("Separation", &lattice.Separation, 0.1f);
    const u32 mn = 1;
    const u32 mx = TKit::MaxThreads;
    ImGui::SliderScalar("Threads", ImGuiDataType_U32, &lattice.Threads, &mn, &mx);

    ImGui::Spacing();
    ImGui::Text("Shape");
    editShape(lattice.Shape, appLayer);
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
