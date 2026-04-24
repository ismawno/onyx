#include "sandbox/sandbox.hpp"
#include "onyx/asset/assets.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/imgui.hpp"
#    include <misc/cpp/imgui_stdlib.h>
#endif
#include "onyx/state/pipelines.hpp"
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
    std::function<std::string(const T &)> GetName = nullptr;
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
            std::string name = options.GetName ? options.GetName(entries[i]) : options.EntryName;
            name = TKit::Format("{} - {}", i, name);
            if (options.Selected)
            {
                if (ImGui::Selectable(name.c_str(), i == *options.Selected))
                    *options.Selected = i;
            }
            else
                ImGui::Text("%s", name.c_str());

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
    AddDefaultMeshes<D2>();
    AddDefaultMeshes<D3>();

    AddDefaultMaterial<D2>();
    AddDefaultMaterial<D3>();

    DefaultSampler = AddSampler("Default sampler").Handle;

    Assets::Upload();

    if (data->Flags & ParseFlag_D2)
    {
        WindowSpecs wspecs{};
        wspecs.Title = "Onyx sandbox window (2D)";
        if (data->Flags & ParseFlag_AddLattice)
            RequestOpenWindow<SandboxWinLayer>(
                [this, data](WindowLayer *l, Window *) {
                    SandboxWinLayer *wlayer = scast<SandboxWinLayer *>(l);
                    AddLattice(wlayer->GetViews<D2>().Views[0].View, data->Lattice2);
                },
                wspecs, D2);
        else
            RequestOpenWindow<SandboxWinLayer>(wspecs, D2);
    }

    if (data->Flags & ParseFlag_D3)
    {
        WindowSpecs wspecs{};
        wspecs.Title = "Onyx sandbox window (3D)";
        if (data->Flags & ParseFlag_AddLattice)
            RequestOpenWindow<SandboxWinLayer>(
                [this, data](WindowLayer *l, Window *) {
                    SandboxWinLayer *wlayer = scast<SandboxWinLayer *>(l);
                    AddLattice(wlayer->GetViews<D3>().Views[0].View, data->Lattice3);
                },
                wspecs, D3);
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

template <typename Vertex>
MeshPoolId<Vertex> &SandboxAppLayer::AddMeshPool(TKit::TierArray<MeshPoolId<Vertex>> &pool, const char *name)
{
    constexpr Dimension D = Vertex::Dim;
    MeshPoolId<Vertex> &mid = pool.Append();
    mid.Handle = Assets::CreateAssetPool<D>(Vertex::Asset);
    mid.Name = name ? name : TKit::Format("Mesh-pool-{:#010x}", mid.Handle);
    return mid;
}

template <typename Vertex>
MeshId<Vertex> &SandboxAppLayer::AddMesh(MeshPoolId<Vertex> &pool, const MeshData<Vertex> &data, const char *name)
{
    const Asset mesh = Assets::CreateMesh(pool.Handle, data);
    MeshId<Vertex> &mid = pool.Elements.Append();
    mid.Handle = mesh;
    mid.Data = data;
    mid.Name = name ? name : TKit::Format("Mesh-{:#010x}", mesh);
    return mid;
}
template <Dimension D> void SandboxAppLayer::AddDefaultMeshes()
{
    auto &meshes = GetMeshes<D>();
    StatMeshPoolId<D> &spool = AddMeshPool(meshes.StatPools, "Default static spool");
    AddMesh(spool, CreateTriangleMeshData<D>(), "Triangle");
    const StatMeshId<D> &quad = AddMesh(spool, CreateQuadMeshData<D>(), "Quad");
    if constexpr (D == D3)
    {
        AddMesh(spool, CreateBoxMeshData(), "Box");
        const StatMeshId<D> &sphere = AddMesh(spool, CreateSphereMeshData(32, 64), "Sphere");
        const StatMeshId<D> &cylinder = AddMesh(spool, CreateCylinderMeshData(64), "Cylinder");
        meshes.DefaultAxesMesh = cylinder.Handle;
        meshes.DefaultLightMesh = sphere.Handle;
    }
    else
        meshes.DefaultAxesMesh = quad.Handle;

    ParaMeshPoolId<D> &ppool = AddMeshPool(meshes.ParaPools, "Default parametric pool");
    AddMesh(ppool, CreateStadiumMeshData<D>(), "Stadium");
    AddMesh(ppool, CreateRoundedQuadMeshData<D>(), "Rounded quad");
    if constexpr (D == D3)
    {
        AddMesh(ppool, CreateCapsuleMeshData(), "Capsule");
        AddMesh(ppool, CreateRoundedBoxMeshData(), "Rounded box");
        AddMesh(ppool, CreateTorusMeshData(), "Torus");
    }
}
template <Dimension D> void SandboxAppLayer::AddDefaultMaterial()
{
    MaterialId<D> &mid = AddMaterial<D>("Default material");
    GetMaterials<D>().DefaultMaterial = mid.Handle;
}

template <Dimension D> static void setShapeProperties(RenderContext<D> *context, const Shape<D> &shape)
{
    context->RenderFlags(shape.Flags);
    context->FillColor(shape.FillColor);

    context->OutlineWidth(shape.OutlineWidth);
    context->OutlineColor(shape.OutlineColor);
    context->Material(shape.Material);
    context->Align(shape.Alignment);

    context->Font(shape.Font);
}
template <Dimension D> static void drawShape(RenderContext<D> *context, const Shape<D> &shape)
{
    if (shape.Geo == Geometry_Circle)
        context->Circle(shape.Transform.ComputeTransform(), shape.CircleParams);
    else if (shape.Geo == Geometry_Static)
        context->StaticMesh(shape.Mesh, shape.Transform.ComputeTransform());
    else if (shape.Geo == Geometry_Parametric)
        context->ParametricMesh(shape.Mesh, shape.Parameters, shape.Transform.ComputeTransform());
    else if (shape.Geo == Geometry_Glyph && !Assets::IsAssetNull(context->GetState().Font))
        context->Text(shape.Text, shape.Transform.ComputeTransform(), shape.TextParams);
}

template <Dimension D> void SandboxAppLayer::DrawShapes()
{
    const auto &contexts = GetContexts<D>();
    for (const ContextData<D> &ctx : contexts.Contexts)
        if (ctx.Flags & SandboxFlag_ContextShouldUpdate)
        {
            ctx.Context->Flush();
            ctx.Context->FontSampler(DefaultSampler);
            ctx.Context->Push();
            for (const Shape<D> &shape : ctx.Shapes)
                if (shape.Geo != Geometry_Glyph || !Assets::IsAssetNull(DefaultSampler))
                {
                    setShapeProperties(ctx.Context, shape);
                    drawShape(ctx.Context, shape);
                }
            ctx.Context->Pop();
            for (const PointLightParameters<D> &pl : ctx.PointLights)
                ctx.Context->PointLight(pl);

            if constexpr (D == D3)
                for (const DirParams &dl : ctx.DirLights)
                    ctx.Context->DirectionalLight(dl.Params);

            if (ctx.Flags & SandboxFlag_DrawLights)
            {
                ctx.Context->Push();
                ctx.Context->Scale(0.01f);
                ctx.Context->Material(ctx.LightMaterial);
                for (const PointLightParameters<D> &pl : ctx.PointLights)
                {
                    ctx.Context->SetTranslation(pl.Position);
                    ctx.Context->FillColor(pl.Tint);
                    if constexpr (D == D2)
                        ctx.Context->Circle();
                    else if (!Assets::IsAssetNull(ctx.LightMesh))
                        ctx.Context->StaticMesh(ctx.LightMesh);
                }
                ctx.Context->Pop();
            }

            const auto &meshes = GetMeshes<D>();
            if ((ctx.Flags & SandboxFlag_DrawAxes) && !meshes.StatPools.IsEmpty())
            {
                ctx.Context->RenderFlags(RenderModeFlag_Shaded);
                ctx.Context->Material(ctx.AxesMaterial);
                if (!Assets::IsAssetNull(ctx.AxesMesh))
                    ctx.Context->Axes(ctx.AxesMesh, {.Thickness = ctx.AxesThickness});
            }
        }
}

template <Dimension D> void SandboxAppLayer::DrawLattices()
{
    const auto &lattices = GetLattices<D>();
    for (const LatticeData<D> &lattice : lattices.Lattices)
        if (lattice.Flags & SandboxFlag_ContextShouldUpdate)
        {
            const Geometry geo = Geometry(lattice.Shape.Geo);
            switch (geo)
            {
            case Geometry_Circle: {
                const CircleParameters opts = lattice.Shape.CircleParams;
                DrawLattice(lattice, [opts](const f32v<D> &pos, RenderContext<D> *context) {
                    context->SetTranslation(pos);
                    context->Circle(opts);
                });
                break;
            }
            case Geometry_Static: {
                const Asset mesh = lattice.Shape.Mesh;
                if (!Assets::IsAssetNull(mesh))
                    DrawLattice(lattice, [mesh](const f32v<D> &pos, RenderContext<D> *context) {
                        context->SetTranslation(pos);
                        context->StaticMesh(mesh);
                    });
                else
                    for (RenderContext<D> *ctx : lattice.Contexts)
                        ctx->Flush();
                break;
            }
            case Geometry_Parametric: {
                const Shape<D> shape = lattice.Shape;
                const Asset mesh = shape.Mesh;
                if (!Assets::IsAssetNull(mesh))
                    DrawLattice(lattice, [mesh, shape](const f32v<D> &pos, RenderContext<D> *context) {
                        context->SetTranslation(pos);
                        context->ParametricMesh(mesh, shape.Parameters);
                    });
                else
                    for (RenderContext<D> *ctx : lattice.Contexts)
                        ctx->Flush();
                break;
            }
            default:
                break;
            };
        }
}

template <Dimension D, typename F> void SandboxAppLayer::DrawLattice(const LatticeData<D> &lattice, F &&fun)
{
    TKit::ITaskManager *tm = GetTaskManager();
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

template <typename Vertex>
static std::string getName(const Asset mesh, const TKit::TierArray<MeshPoolId<Vertex>> &pools)
{
    for (const MeshPoolId<Vertex> &pool : pools)
        for (const MeshId<Vertex> &mid : pool.Elements)
            if (mid.Handle == mesh)
                return mid.Name;
    return "Unknown";
}

template <Dimension D> Shape<D> SandboxAppLayer::CreateShape(const Geometry geo, const Asset mesh)
{
    Shape<D> shape{};
    shape.Geo = geo;
    shape.Mesh = mesh;
    shape.Material = GetMaterials<D>().DefaultMaterial;

    shape.Name = "Unknown";
    const auto &meshes = GetMeshes<D>();
    switch (geo)
    {
    case Geometry_Circle:
        shape.Name = "Circle";
        return shape;
    case Geometry_Static: {
        shape.Name = getName(mesh, meshes.StatPools);
        return shape;
    }
    case Geometry_Parametric: {
        shape.Name = getName(mesh, meshes.ParaPools);
        const ParametricShape stype = Assets::GetParametricShape<D>(mesh);
        if (stype == ParametricShape_Stadium)
            shape.Parameters.Stadium = StadiumParameters{1.f, 0.5f};
        else if (stype == ParametricShape_RoundedQuad)
            shape.Parameters.RoundedQuad = RoundedQuadParameters{1.f, 1.f, 0.5f};
        else if (stype == ParametricShape_Capsule)
            shape.Parameters.Capsule = CapsuleParameters{1.f, 0.5f};
        else if (stype == ParametricShape_RoundedBox)
            shape.Parameters.RoundedBox = RoundedBoxParameters{1.f, 1.f, 1.f, 0.5f};
        else if (stype == ParametricShape_Torus)
            shape.Parameters.Torus = TorusParameters{0.5f, 0.5f};
        return shape;
    }
    case Geometry_Glyph:
        shape.Name = "Text";
        return shape;
    default:
        TKIT_FATAL("[ONYX][SANDBOX] Unknown geometry");
        return shape;
    }
}

template <Dimension D> void SandboxAppLayer::AddContext(const RenderView<D> *view)
{
    RenderContext<D> *context = Renderer::CreateContext<D>();
    auto &contexts = GetContexts<D>();
    ContextData<D> &data = contexts.Contexts.Append();
    data.Context = context;
    data.AxesMaterial = GetMaterials<D>().DefaultMaterial;
    data.LightMaterial = GetMaterials<D>().DefaultMaterial;
    data.AxesMesh = GetMeshes<D>().DefaultAxesMesh;
    if constexpr (D == D3)
    {
        data.Flags |= SandboxFlag_DrawAxes;
        data.DirLights.Append();
        data.LightMesh = GetMeshes<D3>().DefaultLightMesh;
    }
    if (view)
        context->AddTarget(view);
}

template <Dimension D> void SandboxAppLayer::AddLattice(const RenderView<D> *view, const LatticeData<D> &lattice)
{
    auto &lattices = GetLattices<D>();
    LatticeData<D> &data = lattices.Lattices.Append(lattice);
    for (u32 i = 0; i < TKit::MaxThreads; ++i)
    {
        RenderContext<D> *ctx = Renderer::CreateContext<D>();
        if (view)
            ctx->AddTarget(view);
        data.Contexts[i] = ctx;
    }
    data.Shape = CreateShape<D>(data.Geo);
}

template <Dimension D> MaterialId<D> &SandboxAppLayer::AddMaterial(const char *name)
{
    auto &materials = GetMaterials<D>();
    MaterialId<D> &mat = materials.Elements.Append();
    mat.Handle = Assets::CreateMaterial(mat.Data);
    mat.Name = name ? name : TKit::Format("Material-{:#010x}", mat.Handle);
    return mat;
}

SamplerId &SandboxAppLayer::AddSampler(const char *name)
{
    SamplerId &sampler = Samplers.Append();
    sampler.Handle = Assets::CreateSampler(sampler.Data);
    sampler.Name = name ? name : TKit::Format("Sampler-{:#010x}", sampler.Handle);
    return sampler;
}

void SandboxAppLayer::AddTexture(const ImageData &data, const char *name)
{
    TextureId &tex = Textures.Append();
    tex.Handle = Assets::CreateTexture(data);
    tex.Name = name ? name : TKit::Format("Texture-{:#010x}", tex.Handle);
}

void SandboxAppLayer::AddFontPool(const char *name)
{
    FontPoolId &pool = FontPools.Append();
    pool.Handle = Assets::CreateFontPool();
    pool.Name = name ? name : TKit::Format("Font-pool-{:#010x}", pool.Handle);
}

void SandboxAppLayer::AddFont(FontPoolId &pool, const FontData &data, const char *name)
{
    FontId &font = pool.Elements.Append();
    font.Handle = Assets::CreateFont(pool.Handle, data);
    font.Name = name ? name : TKit::Format("Font-{:#010x}", font.Handle);

    TextureId &tex = Textures.Append();
    tex.Handle = Assets::GetFontAtlas(font.Handle);
    tex.Name = TKit::Format("Font-atlas-{:#010x}", tex.Handle);
}

template <Dimension D> void SandboxAppLayer::UpdateMaterialData()
{
    auto &materials = GetMaterials<D>();
    for (MaterialId<D> &mat : materials.Elements)
        mat.Data = Assets::GetMaterialData<D>(mat.Handle);
}

SandboxWinLayer::SandboxWinLayer(ApplicationLayer *appLayer, Window *window, const Dimension dim)
    : WindowLayer(appLayer, window, {.Flags = WindowLayerFlag_ImGuiEnabled})
{
    SandboxAppLayer *alayer = GetApplicationLayer<SandboxAppLayer>();
    if (dim == D2)
    {
        Camera<D2> *cam = AddCamera<D2>();
        RenderView<D2> *view = AddView(cam);
        alayer->AddContext<D2>(view);
        TabSelect = 1;
    }
    else
    {
        Camera<D3> *cam = AddCamera<D3>();
        RenderView<D3> *view = AddView(cam);
        alayer->AddContext<D3>(view);
        TabSelect = 2;
    }
}
SandboxWinLayer::~SandboxWinLayer()
{
    const auto wait = [](TKit::Task<Dialog::Result<Dialog::Path>> &task) {
        if (task)
        {
            TKit::ITaskManager *tm = GetTaskManager();
            tm->WaitUntilFinished(task);
        }
    };
    wait(StatMeshTask);
    wait(TexTask);
    wait(FontTask);
    wait(GltfTask);
}

void SandboxWinLayer::OnRender(const DeltaTime &deltaTime)
{
    TKIT_PROFILE_NSCOPE("Onyx::Sandbox::OnRender");
    const Window *win = GetWindow();

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        RenderView<D2> *rv2 = win->GetMouseRenderView<D2>();
        RenderView<D3> *rv3 = win->GetMouseRenderView<D3>();

        if (rv2)
            win->ControlCamera(deltaTime.Measured, rv2->GetCamera());
        if (rv3)
            win->ControlCamera(deltaTime.Measured, rv3->GetCamera());
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
template <typename T> static bool combo(const char *name, T *index, const char *items)
{
    i32 idx = i32(*index);
    if (ImGui::Combo(name, &idx, items))
    {
        *index = T(idx);
        return true;
    }
    return false;
}
template <typename T> static bool combo(const char *name, T *index, const TKit::Span<const char *const> items)
{
    i32 idx = i32(*index);
    const i32 size = i32(items.GetSize());
    if (ImGui::Combo(name, &idx, items.GetData(), size))
    {
        *index = T(idx);
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

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
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
        if (ImGui::Button("Reload shaders"))
        {
            Pipelines::ReloadShaders();
            Renderer::ReloadPipelines();
        }
    }
    ImGui::End();

    if (ImGui::Begin("Editor"))
    {
        ImGui::Text("This is the editor panel, where you can interact with the demo.");
        ImGui::TextWrapped(
            "Onyx windows can draw shapes in 2D and 3D, and have a separate API for each even though the "
            "window is shared. Users interact with the rendering API through rendering contexts.");
        PresentModeEditor(window, EditorFlag_DisplayHelp);

        const f32v2 spos = window->GetScreenMousePosition();
        ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos[0], spos[1]);
        HelpMarkerSameLine("The screen mouse position is always Math::Normalized to the window size, always ranging "
                           "from -1 to 1 for 'x' and 'y', and from 0 to 1 for 'z'.");

        ImGui::BeginTabBar("Dimension");
        if (ImGui::BeginTabItem("2D", nullptr, TabSelect == 1 ? ImGuiTabItemFlags_SetSelected : 0))
        {
            RenderContexts<D2>();
            RenderCameras<D2>();
            RenderWindowViews<D2>();
            RenderMeshPools<D2>();
            RenderMaterials<D2>();
            RenderSamplers();
            RenderTextures();
            RenderFontPools();
            RenderLattices<D2>();
            RenderRenderer<D2>();
            RenderGltf<D2>();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("3D", nullptr, TabSelect == 2 ? ImGuiTabItemFlags_SetSelected : 0))
        {
            RenderContexts<D3>();
            RenderCameras<D3>();
            RenderWindowViews<D3>();
            RenderMeshPools<D3>();
            RenderMaterials<D3>();
            RenderSamplers();
            RenderTextures();
            RenderFontPools();
            RenderLattices<D3>();
            RenderRenderer<D3>();
            RenderGltf<D3>();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        TabSelect = 0;
    }
    ImGui::End();
}

template <Dimension D> void SandboxWinLayer::RenderCameras()
{
    if (ImGui::CollapsingHeader("Cameras"))
    {
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
        opts.OnRemoval = [this](const CameraData<D> &camera) {
            auto &views = GetViews<D>();
            for (u32 i = 0; i < views.Views.GetSize(); ++i)
                if (views.Views[i].View->GetCamera() == camera.Camera)
                {
                    GetWindow()->DestroyRenderView(views.Views[i].View);
                    views.Views.RemoveOrdered(views.Views.begin() + i);
                }

            TKit::TierAllocator *tier = TKit::GetTier();
            tier->Destroy(camera.Camera);
        };
        renderEntries(cameras.Cameras, opts);
    }
}

template <Dimension D> void SandboxWinLayer::RenderCamera(CameraData<D> &camera)
{
    Camera<D> *cam = camera.Camera;
    const Transform<D> &view = cam->View;
    ImGui::Text("View transform");
    HelpMarkerSameLine(
        "The view transform are the coordinates of the camera, detached from any render context coordinate system.");

    DisplayTransform(view, EditorFlag_DisplayHelp);
    if constexpr (D == D3)
    {
        combo("Projection", &cam->Mode, "Orthographic\0Perspective\0\0");
        OrthographicParameters<D3> &ort = cam->OrthoParameters;
        PerspectiveParameters &pers = cam->PerspParameters;

        if (cam->Mode == CameraMode_Perspective)
        {
            f32 degs = Math::Degrees(pers.FieldOfView);

            if (ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f))
                pers.FieldOfView = Math::Radians(degs);

            ImGui::SliderFloat("Near", &pers.Near, 0.1f, 10.f);
            ImGui::SliderFloat("Far", &pers.Far, 10.f, 100.f);
        }
        else
        {
            ImGui::SliderFloat("Size", &ort.Size, 0.f, 10.f);
            ImGui::SliderFloat("Near", &ort.Near, -10.f, 10.f);
            ImGui::SliderFloat("Far", &ort.Far, -10.f, 10.f);
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

template <Dimension D>
static bool cameraCombo(const char *name, u32 *index, const TKit::TierArray<CameraData<D>> &cameras)
{
    TKit::StackArray<const char *> names{};
    names.Reserve(cameras.GetSize());

    for (const CameraData<D> &cam : cameras)
        names.Append(cam.Name.c_str());

    return combo(name, index, names);
}

template <Dimension D>
static bool renderViewCombo(const char *name, u32 *index, const TKit::TierArray<ViewData<D>> &views)
{
    TKit::StackArray<const char *> names{};
    names.Reserve(views.GetSize());

    for (const ViewData<D> &view : views)
        names.Append(view.Name.c_str());

    if (*index != TKIT_U32_MAX)
    {
        if (ImGui::Button("X##RenderView"))
        {
            *index = TKIT_U32_MAX;
            return true;
        }
        ImGui::SameLine();
    }
    return combo(name, index, names);
}

template <Dimension D> void SandboxWinLayer::RenderWindowViews()
{
    if (ImGui::CollapsingHeader("Views"))
    {
        auto &views = GetViews<D>();
        auto &cams = GetCameras<D>();

        cameraCombo("Camera", &cams.CameraIndex, cams.Cameras);

        ImGui::BeginDisabled(cams.CameraIndex >= cams.Cameras.GetSize());
        if (ImGui::Button("Add view"))
            AddView<D>(cams.Cameras[cams.CameraIndex].Camera);
        ImGui::EndDisabled();

        EntriesOptions<ViewData<D>> opts{};
        opts.EntryName = "View";
        opts.Selected = &views.Active;
        opts.OnSelected = [this](ViewData<D> &view) { RenderWindowView(view); };
        opts.OnRemoval = [this](const ViewData<D> &view) { GetWindow()->DestroyRenderView(view.View); };
        renderEntries(views.Views, opts);
    }
}

template <Dimension D> void SandboxWinLayer::RenderWindowView(ViewData<D> &view)
{
    const Window *win = GetWindow();
    RenderView<D> *v = view.View;
    const f32v2 mpos = win->GetScreenMousePosition();
    const f32v2 vpos = v->ScreenToViewport(mpos);
    ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos[0], vpos[1]);
    ImGui::Text("Is mouse within this viewport: %s", v->IsWithinViewport(mpos) ? "True" : "False");

    auto &cameras = GetCameras<D>();
    if (cameraCombo("Camera##View", &view.CameraIndex, cameras.Cameras))
        view.View->SetCamera(cameras.Cameras[view.CameraIndex].Camera);

    if constexpr (D == D2)
    {
        const f32v2 wpos = v->ViewportToWorld(vpos);
        ImGui::Text("World mouse position: (%.2f, %.2f)", wpos[0], wpos[1]);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &view.ZOffset, 0.f, 1.f);
        HelpMarkerSameLine(
            "In 3D, the world mouse position can be ambiguous because of the extra dimension. This amibiguity needs to "
            "somehow be resolved. In most use-cases, ray casting is the best approach to fully define this position, "
            "but because this is a simple demo, the z offset can be manually specified, and is in the range [0, 1] "
            "(screen coordinates). Note that, if in perspective mode, 0 corresponds to the near plane and 1 to the "
            "far plane.");

        const f32v3 wpos = v->ViewportToWorld(f32v3{vpos, view.ZOffset});
        ImGui::Text("World mouse position: (%.2f, %.2f, %.2f)", wpos[0], wpos[1], wpos[2]);
    }
    ImGui::ColorEdit3("Clear color", v->ClearColor.GetData());

    ImGui::Text("Viewport");
    ImGui::SameLine();
    ScreenViewport viewport = v->GetViewport();
    if (ViewportEditor(viewport, EditorFlag_DisplayHelp))
        v->SetViewport(viewport);

    ImGui::Text("Scissor");
    ImGui::SameLine();
    ScreenScissor scissor = v->GetScissor();
    if (ScissorEditor(scissor, EditorFlag_DisplayHelp))
        v->SetScissor(scissor);
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

template <typename T>
static bool poolMemberNameCombo(const char *name, const TKit::TierArray<T> &container, Asset *handle)
{
    ImGui::PushID(handle);
    TKit::StackArray<const char *> names{};

    u32 size = 0;
    for (const T &pool : container)
        size += pool.Elements.GetSize();

    names.Reserve(size);

    u32 idx = TKIT_U32_MAX;
    u32 gidx = 0;
    for (const T &pool : container)
        for (const auto &elm : pool.Elements)
        {
            names.Append(elm.Name.c_str());
            if (elm.Handle == *handle)
                idx = gidx;
            ++gidx;
        }

    bool changed = false;
    if (!Assets::IsAssetNull(*handle))
    {
        changed |= ImGui::Button("X");
        if (changed)
        {
            *handle = NullHandle;
            idx = TKIT_U32_MAX;
        }
        ImGui::SameLine();
    }
    ImGui::PopID();
    if (combo(name, &idx, names))
    {
        for (const T &pool : container)
            if (idx < pool.Elements.GetSize())
            {
                *handle = pool.Elements[idx].Handle;
                break;
            }
            else
                idx -= pool.Elements.GetSize();

        changed = true;
    }
    return changed;
}

template <Dimension D> static bool statMeshNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *mesh)
{
    return poolMemberNameCombo(name, appLayer->GetMeshes<D>().StatPools, mesh);
}
template <Dimension D> static bool paraMeshNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *mesh)
{
    return poolMemberNameCombo(name, appLayer->GetMeshes<D>().ParaPools, mesh);
}

static bool fontNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *font)
{
    return poolMemberNameCombo(name, appLayer->FontPools, font);
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
        if (elm.Handle == *handle)
            idx = i;
    }
    bool changed = false;
    if (!Assets::IsAssetNull(*handle))
    {
        changed |= ImGui::Button("X");
        if (changed)
        {
            *handle = NullHandle;
            idx = TKIT_U32_MAX;
        }
        ImGui::SameLine();
    }
    ImGui::PopID();
    if (combo(name, &idx, names))
    {
        *handle = container[idx].Handle;
        changed = true;
    }
    return changed;
}

template <Dimension D> static bool matNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *material)
{
    return nameCombo(name, appLayer->GetMaterials<D>().Elements, material);
}
static bool texNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *handle)
{
    return nameCombo(name, appLayer->Textures, handle);
}
static bool sampNameCombo(const char *name, SandboxAppLayer *appLayer, Asset *handle)
{
    return nameCombo(name, appLayer->Samplers, handle);
}

template <Dimension D> void SandboxWinLayer::RenderContext(ContextData<D> &context)
{
    ImGui::CheckboxFlags("Continuous update##Context", &context.Flags, SandboxFlag_ContextShouldUpdate);
    if (ImGui::TreeNode("Targets"))
    {
        auto &views = GetViews<D>();
        for (const ViewData<D> &vd : views.Views)
        {
            const ViewMask vbit = vd.View->GetViewBit();
            bool targets = vbit & context.Context->GetViewMask();
            ImGui::PushID(vbit);
            if (ImGui::Checkbox("Target: ##Context", &targets))
            {
                if (targets)
                    context.Context->AddTarget(vbit);
                else
                    context.Context->RemoveTarget(vbit);
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Text("%s", vd.Name.c_str());
        }
        ImGui::TreePop();
    }

    ImGui::CheckboxFlags("Draw axes", &context.Flags, SandboxFlag_DrawAxes);
    if (context.Flags & SandboxFlag_DrawAxes)
    {
        ImGui::SliderFloat("Axes thickness", &context.AxesThickness, 0.001f, 0.1f);
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        statMeshNameCombo<D>("Axes shape", appLayer, &context.AxesMesh);
        matNameCombo<D>("Axes material", appLayer, &context.AxesMaterial);
    }

    ImGui::Spacing();
    ImGui::Text("Shape picker");
    RenderShapePicker<D>(context);
    ImGui::Spacing();
    ImGui::Text("Light picker");
    ImGui::CheckboxFlags("Draw lights", &context.Flags, SandboxFlag_DrawLights);
    if (context.Flags & SandboxFlag_DrawLights)
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        if constexpr (D == D3)
            statMeshNameCombo<D>("Lights shape", appLayer, &context.LightMesh);
        matNameCombo<D>("Lights material", appLayer, &context.LightMaterial);
    }
    RenderLightPicker<D>(context);
}

template <Dimension D> static void editShape(Shape<D> &shape, SandboxAppLayer *appLayer)
{
    ImGui::PushID(&shape);
    ImGui::Text("Transform");
    ImGui::SameLine();
    TransformEditor<D>(shape.Transform, EditorFlag_DisplayHelp);
    ImGui::CheckboxFlags("Fill", &shape.Flags, RenderModeFlag_Shaded);
    ImGui::CheckboxFlags("Outline", &shape.Flags, RenderModeFlag_Outlined);
    ImGui::ColorEdit4("Fill color", shape.FillColor.GetData());
    ImGui::ColorEdit4("Outline color", shape.OutlineColor.GetData());
    ImGui::SliderFloat("Outline width", &shape.OutlineWidth, 0.4f, 1.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    combo("X Alignment", &shape.Alignment[0], "Left\0Center\0Right\0None\0\0");
    combo("Y Alignment", &shape.Alignment[1], "Bottom\0Center\0Top\0None\0\0");
    if constexpr (D == D3)
        combo("Z Alignment", &shape.Alignment[2], "Near\0Center\0Far\0None\0\0");

    matNameCombo<D>("Material##Shape", appLayer, &shape.Material);

    if (shape.Geo == Geometry_Circle)
    {
        ImGui::SliderFloat("Inner fade", &shape.CircleParams.InnerFade, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Outer fade", &shape.CircleParams.OuterFade, 0.f, 1.f, "%.2f");
        ImGui::SliderAngle("Lower angle", &shape.CircleParams.LowerAngle);
        ImGui::SliderAngle("Upper angle", &shape.CircleParams.UpperAngle);
        ImGui::SliderFloat("Hollowness", &shape.CircleParams.Hollowness, 0.f, 1.f, "%.2f");
    }
    else if (shape.Geo == Geometry_Parametric)
    {
        const ParametricShape stype = Assets::GetParametricShape<D>(shape.Mesh);
        switch (stype)
        {
        case ParametricShape_Stadium: {
            StadiumParameters &params = shape.Parameters.Stadium;
            ImGui::DragFloat("Height", &params.Height, 0.04f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Radius", &params.Radius, 0.04f, 0.f, TKIT_F32_MAX);
            break;
        }
        case ParametricShape_RoundedQuad: {
            RoundedQuadParameters &params = shape.Parameters.RoundedQuad;
            ImGui::DragFloat("Width", &params.Width, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Height", &params.Height, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Radius", &params.Radius, 0.01f, 0.f, TKIT_F32_MAX);
            break;
        }
        case ParametricShape_Capsule: {
            CapsuleParameters &params = shape.Parameters.Capsule;
            ImGui::DragFloat("Height", &params.Height, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Radius", &params.Radius, 0.01f, 0.f, TKIT_F32_MAX);
            break;
        }
        case ParametricShape_RoundedBox: {
            RoundedBoxParameters &params = shape.Parameters.RoundedBox;
            ImGui::DragFloat("Width", &params.Width, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Height", &params.Height, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Length", &params.Length, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("Radius", &params.Radius, 0.01f, 0.f, TKIT_F32_MAX);
            break;
        }
        case ParametricShape_Torus: {
            TorusParameters &params = shape.Parameters.Torus;
            ImGui::DragFloat("InnerRadius", &params.InnerRadius, 0.01f, 0.f, TKIT_F32_MAX);
            ImGui::DragFloat("OuterRadius", &params.OuterRadius, 0.01f, 0.f, TKIT_F32_MAX);
            break;
        }
        default:
            TKIT_FATAL("[ONYX][SANDBOX] Unrecognized shape");
        }
    }
    else if (shape.Geo == Geometry_Glyph)
    {
        fontNameCombo("Font", appLayer, &shape.Font);
        ImGui::BeginDisabled(Assets::IsAssetNull(shape.Font));
        ImGui::InputText("Text", &shape.Text);
        ImGui::EndDisabled();

        ImGui::DragFloat("Kerning", &shape.TextParams.Kerning, 0.01f);
        ImGui::DragFloat("Line spacing", &shape.TextParams.LineSpacing, 0.01f);
        ImGui::DragFloat("Width", &shape.TextParams.Width, 0.01f);
    }
    ImGui::PopID();
}

template <Dimension D> void SandboxWinLayer::RenderShapePicker(ContextData<D> &context)
{
    combo("Geometry##Picker", &context.GeometryToSpawn, "Circle\0Static mesh\0Parametric mesh\0Text\0\0");
    const Geometry geo = context.GeometryToSpawn;

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();

    if (geo == Geometry_Static)
        statMeshNameCombo<D>("Shape##Picker", appLayer, &context.MeshToSpawn[geo]);
    else if (geo == Geometry_Parametric)
        paraMeshNameCombo<D>("Shape##Picker", appLayer, &context.MeshToSpawn[geo]);

    ImGui::BeginDisabled(geo != Geometry_Circle && Assets::IsAssetNull(context.MeshToSpawn[geo]));
    if (ImGui::Button("Spawn##Shape"))
        context.Shapes.Append(appLayer->CreateShape<D>(geo, context.MeshToSpawn[geo]));
    ImGui::EndDisabled();

    EntriesOptions<Shape<D>> opts{};
    opts.TreeName = "Shapes";
    opts.Selected = &context.SelectedShape;
    opts.OnSelected = [appLayer](Shape<D> &shape) { editShape<D>(shape, appLayer); };
    opts.GetName = [](const Shape<D> &shape) { return shape.Name; };
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
            context.PointLights.Append();
        if constexpr (D == D3)
            if (ltype == Light_Directional)
                context.DirLights.Append();
    }

    EntriesOptions<PointLightParameters<D>> popts{};
    popts.TreeName = "Point lights";
    popts.EntryName = "Point light";
    popts.Selected = &context.SelectedPointLight;
    popts.OnSelected = [](PointLightParameters<D> &light) { PointLightEditor(light, EditorFlag_DisplayHelp); };
    renderEntries(context.PointLights, popts);

    if constexpr (D == D3)
    {
        EntriesOptions<DirParams> dopts{};
        dopts.TreeName = "Directional lights";
        dopts.EntryName = "Directional light";
        dopts.Selected = &context.SelectedDirLight;
        dopts.OnSelected = [this](DirParams &light) {
            auto &views = GetViews<D>();
            if (renderViewCombo("View to fit", &light.RenderViewIndex, views.Views))
                light.Params.Cascades.View =
                    light.RenderViewIndex == TKIT_U32_MAX ? nullptr : views.Views[light.RenderViewIndex].View;

            DirectionalLightEditor(light.Params, EditorFlag_DisplayHelp);
        };
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
            appLayer->AddLattice<D>(GetMainView<D>());

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

template <Dimension D> void SandboxWinLayer::RenderMeshPools()
{
    if (ImGui::CollapsingHeader("Meshes"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &meshes = appLayer->GetMeshes<D>();
        if (ImGui::TreeNode("Static mesh pools"))
        {
            if (ImGui::Button("Add##Static"))
                appLayer->AddMeshPool(meshes.StatPools);

            EntriesOptions<StatMeshPoolId<D>> opts{};
            opts.Selected = &meshes.Active;
            opts.OnSelected = [this](StatMeshPoolId<D> &pool) { RenderMeshPool(pool); };
            opts.GetName = [](const StatMeshPoolId<D> &pool) { return pool.Name; };
            opts.OnRemoval = [&meshes, appLayer](const StatMeshPoolId<D> &pool) {
                Assets::DestroyAssetPool<D>(pool.Handle);
                auto &contexts = appLayer->GetContexts<D>();
                for (ContextData<D> &ctx : contexts.Contexts)
                {
                    for (u32 i = ctx.Shapes.GetSize() - 1; i < ctx.Shapes.GetSize(); --i)
                        if (Assets::GetAssetPool(ctx.Shapes[i].Mesh) == pool.Handle)
                            ctx.Shapes.RemoveOrdered(ctx.Shapes.begin() + i);
                    if (Assets::GetAssetPool(ctx.AxesMesh) == pool.Handle)
                        ctx.AxesMesh = NullAsset;

                    if constexpr (D == D3)
                        if (Assets::GetAssetPool(ctx.LightMesh) == pool.Handle)
                            ctx.LightMesh = NullAsset;
                }
                auto &lattices = appLayer->GetLattices<D>();
                for (LatticeData<D> &lattice : lattices.Lattices)
                {
                    if (Assets::GetAssetPool(lattice.StatMesh) == pool.Handle)
                        lattice.StatMesh = NullAsset;
                    if (Assets::GetAssetPool(lattice.Shape.Mesh) == pool.Handle)
                        lattice.Shape.Mesh = NullAsset;
                }
                if (Assets::GetAssetPool(meshes.DefaultAxesMesh) == pool.Handle)
                    meshes.DefaultAxesMesh = NullAsset;
                if constexpr (D == D3)
                    if (Assets::GetAssetPool(meshes.DefaultLightMesh) == pool.Handle)
                        meshes.DefaultLightMesh = NullAsset;

                Assets::RequestUpload();
            };
            ImGui::SameLine();
            ImGui::TextDisabled("Removing the default mesh pool may lead to weird behaviour!");
            renderEntries(meshes.StatPools, opts);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Parametric mesh pools"))
        {
            if (ImGui::Button("Add##Parametric"))
                appLayer->AddMeshPool(meshes.ParaPools);
            EntriesOptions<ParaMeshPoolId<D>> opts{};
            opts.Selected = &meshes.Active;
            opts.OnSelected = [this](ParaMeshPoolId<D> &pool) { RenderMeshPool(pool); };
            opts.GetName = [](const ParaMeshPoolId<D> &pool) { return pool.Name; };
            opts.OnRemoval = [appLayer](const ParaMeshPoolId<D> &pool) {
                Assets::DestroyAssetPool<D>(pool.Handle);
                auto &contexts = appLayer->GetContexts<D>();
                for (ContextData<D> &ctx : contexts.Contexts)
                    for (u32 i = ctx.Shapes.GetSize() - 1; i < ctx.Shapes.GetSize(); --i)
                        if (Assets::GetAssetPool(ctx.Shapes[i].Mesh) == pool.Handle)
                            ctx.Shapes.RemoveOrdered(ctx.Shapes.begin() + i);

                auto &lattices = appLayer->GetLattices<D>();
                for (LatticeData<D> &lattice : lattices.Lattices)
                    if (Assets::GetAssetPool(lattice.Shape.Mesh) == pool.Handle)
                        lattice.Shape.Mesh = NullAsset;
                Assets::RequestUpload();
            };
            renderEntries(meshes.ParaPools, opts);
            ImGui::TreePop();
        }
    }
}

template <Dimension D> void SandboxWinLayer::RenderMaterials()
{
    if (ImGui::CollapsingHeader("Materials"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        auto &materials = appLayer->GetMaterials<D>();
        if (ImGui::Button("Add material"))
        {
            appLayer->AddMaterial<D>();
            Assets::RequestUpload();
        }

        EntriesOptions<MaterialId<D>> opts{};
        opts.Selected = &materials.Active;
        opts.OnSelected = [this](MaterialId<D> &material) { RenderMaterial(material); };
        opts.GetName = [](const MaterialId<D> &material) { return material.Name; };

        opts.OnRemoval = [&materials, appLayer](MaterialId<D> &mat) {
            Assets::DestroyMaterial<D>(mat.Handle);
            auto &contexts = appLayer->GetContexts<D>();
            for (ContextData<D> &ctx : contexts.Contexts)
            {
                for (Shape<D> &shape : ctx.Shapes)
                    if (shape.Material == mat.Handle)
                        shape.Material = NullAsset;
                if (ctx.AxesMaterial == mat.Handle)
                    ctx.AxesMaterial = NullAsset;
                if (ctx.LightMaterial == mat.Handle)
                    ctx.LightMaterial = NullAsset;
            }
            auto &lattices = appLayer->GetLattices<D>();
            for (LatticeData<D> &lattice : lattices.Lattices)
                if (lattice.Shape.Material == mat.Handle)
                    lattice.Shape.Material = NullAsset;
            if (materials.DefaultMaterial == mat.Handle)
                materials.DefaultMaterial = NullAsset;
            Assets::RequestUpload();
        };

        renderEntries(materials.Elements, opts);
    }
}

template <Dimension D> void SandboxWinLayer::RenderMaterial(MaterialId<D> &material)
{
    ImGui::InputText("Name##Material", &material.Name);
    ImGui::Text("Material handle: %s", TKit::Format("{:#010x}", material.Handle).c_str());
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
        Assets::UpdateMaterial(material.Handle, material.Data);
        Assets::RequestUpload();
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
    TKit::ITaskManager *tm = GetTaskManager();
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
        {
            appLayer->AddSampler();
            Assets::RequestUpload();
        }

        ImGui::TextDisabled(
            "The default sampler is used to render text! Removing it will prevent text from being rendered");

        EntriesOptions<SamplerId> opts{};
        opts.GetName = [](const SamplerId &samp) { return samp.Name; };
        opts.OnRemoval = [appLayer](SamplerId &samp) {
            Assets::DestroySampler(samp.Handle);
            appLayer->UpdateMaterialData<D2>();
            appLayer->UpdateMaterialData<D3>();
            if (appLayer->DefaultSampler == samp.Handle)
                appLayer->DefaultSampler = NullHandle;
            Assets::RequestUpload();
        };
        opts.OnSelected = [this](SamplerId &samp) { RenderSampler(samp); };
        opts.Selected = &appLayer->SelectedSampler;
        renderEntries(appLayer->Samplers, opts);
    }
}

void SandboxWinLayer::RenderSampler(SamplerId &sampler)
{
    ImGui::InputText("Name##Sampler", &sampler.Name);
    ImGui::Text("Sampler handle: %s", TKit::Format("{:#010x}", sampler.Handle).c_str());
    if (SamplerEditor(sampler.Data, EditorFlag_DisplayHelp))
    {
        Assets::UpdateSampler(sampler.Handle, sampler.Data);
        Assets::RequestUpload();
    }
}

void SandboxWinLayer::RenderTextures()
{
    if (ImGui::CollapsingHeader("Textures"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        HandleLoadDialog(TexTask, [&](const Dialog::Path &path) {
            const auto res = LoadImageDataFromFile(path.string().c_str(), ImageComponent_RGBA);
            VKIT_LOG_RESULT_ERROR(res);
            if (!res)
                return;

            const ImageData &data = res.GetValue();
            appLayer->AddTexture(data, path.filename().string().c_str());
            Assets::RequestUpload();
        });

        EntriesOptions<TextureId> opts{};
        opts.GetName = [](const TextureId &tex) { return tex.Name; };
        opts.OnSelected = [](TextureId &tex) {
            ImGui::Text("Texture handle: %s", TKit::Format("{:#010x}", tex.Handle).c_str());
        };

        opts.Selected = &appLayer->SelectedTexture;
        opts.OnRemoval = [appLayer](TextureId &tex) {
            Assets::DestroyTexture(tex.Handle);
            appLayer->UpdateMaterialData<D2>();
            appLayer->UpdateMaterialData<D3>();
            Assets::RequestUpload();
        };
        renderEntries(appLayer->Textures, opts);
    }
}

void SandboxWinLayer::RenderFontPools()
{
    if (ImGui::CollapsingHeader("Fonts"))
    {
        SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
        if (ImGui::Button("Add pool##Font"))
            appLayer->AddFontPool();

        EntriesOptions<FontPoolId> opts{};
        opts.Selected = &appLayer->SelectedFontPool;
        opts.OnSelected = [this](FontPoolId &pool) { RenderFontPool(pool); };
        opts.GetName = [](const FontPoolId &pool) { return pool.Name; };
        opts.OnRemoval = [appLayer](FontPoolId &pool) {
            Assets::DestroyFontPool(pool.Handle);
            auto &ctx2 = appLayer->GetContexts<D2>();
            for (ContextData<D2> &ctx : ctx2.Contexts)
                for (Shape<D2> &shape : ctx.Shapes)
                    if (Assets::GetAssetPool(shape.Font) == pool.Handle)
                        shape.Font = NullHandle;
            auto &ctx3 = appLayer->GetContexts<D3>();
            for (ContextData<D3> &ctx : ctx3.Contexts)
                for (Shape<D3> &shape : ctx.Shapes)
                    if (Assets::GetAssetPool(shape.Font) == pool.Handle)
                        shape.Font = NullHandle;
            Assets::RequestUpload();
        };
        renderEntries(appLayer->FontPools, opts);
    }
}

void SandboxWinLayer::RenderFontPool(FontPoolId &pool)
{
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    HandleLoadDialog(FontTask, [&](const Dialog::Path &path) {
        const auto res = LoadFontDataFromFile(path.string().c_str());
        VKIT_LOG_RESULT_ERROR(res);
        if (!res)
            return;

        const FontData &data = res.GetValue();
        appLayer->AddFont(pool, data, path.filename().string().c_str());
        Assets::RequestUpload();
    });

    EntriesOptions<FontId> opts{};
    opts.GetName = [](const FontId &font) { return font.Name; };
    opts.OnSelected = [](FontId &font) {
        ImGui::Text("Font handle: %s", TKit::Format("{:#010x}", font.Handle).c_str());
    };
    opts.Selected = &appLayer->SelectedTexture;
    opts.RemoveButton = false;
    renderEntries(pool.Elements, opts);
}

template <Dimension D> void SandboxWinLayer::RenderGltf()
{
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    HandleLoadDialog(
        GltfTask,
        [&](const Dialog::Path &path) {
            auto res = LoadGltfDataFromFile<D>(path.string(), LoadGltfDataFlag_ForceRGBA);
            VKIT_LOG_RESULT_ERROR(res);
            if (!res)
                return;

            StatMeshPoolId<D> &mspool = appLayer->AddMeshPool(appLayer->GetMeshes<D>().StatPools);
            GltfData<D> &data = res.GetValue();
            const GltfHandles handles = Assets::CreateGltfAssets(mspool.Handle, data);
            Assets::RequestUpload();

            for (u32 i = 0; i < handles.StaticMeshes.GetSize(); ++i)
            {
                const Asset mesh = handles.StaticMeshes[i];
                const StatMeshData<D> &mdat = data.StaticMeshes[i];
                StatMeshId<D> &mid = mspool.Elements.Append();
                mid.Name = TKit::Format("GLTF-Mesh-{:#010x}", mesh);
                mid.Handle = mesh;
                mid.Data = mdat;
            }

            for (u32 i = 0; i < handles.Materials.GetSize(); ++i)
            {
                const Asset mat = handles.Materials[i];
                const MaterialData<D> &mdat = data.Materials[i];
                MaterialId<D> &mid = appLayer->GetMaterials<D>().Elements.Append();
                mid.Name = TKit::Format("GLTF-Material-{:#010x}", mat);
                mid.Handle = mat;
                mid.Data = mdat;
            }

            auto &samplers = appLayer->Samplers;
            for (u32 i = 0; i < handles.Samplers.GetSize(); ++i)
            {
                const Asset samp = handles.Samplers[i];
                const SamplerData &sdata = data.Samplers[i];
                SamplerId &sid = samplers.Append();
                sid.Name = TKit::Format("GLTF-Sampler-{:#010x}", samp);
                sid.Handle = samp;
                sid.Data = sdata;
            }

            auto &textures = appLayer->Textures;
            for (const Asset tex : handles.Textures)
            {
                TextureId &tid = textures.Append();
                tid.Name = TKit::Format("GLTF-Texture-{:#010x}", tex);
                tid.Handle = tex;
            }
        },
        "Load GLTF file");
    ImGui::SameLine();
    ImGui::TextDisabled("A new mesh pool will be created for each GLTF load");
}

template <typename Vertex> void SandboxWinLayer::RenderMeshPool(MeshPoolId<Vertex> &pool)
{
    constexpr Dimension D = Vertex::Dim;
    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    auto &meshes = appLayer->GetMeshes<D>();

    EntriesOptions<MeshId<Vertex>> opts{};
    // opts.TreeName = "Meshes";
    opts.Selected = &pool.Active;
    opts.OnSelected = [this](MeshId<Vertex> &mesh) { RenderMesh(mesh); };
    opts.GetName = [](const MeshId<Vertex> &mesh) { return mesh.Name; };
    opts.RemoveButton = false;
    renderEntries(pool.Elements, opts);

    if constexpr (std::is_same_v<Vertex, StatVertex<D>>)
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
            {
                appLayer->AddMesh(pool, CreateRegularPolygonMeshData<D>(meshes.RegularPolySides),
                                  name[0] ? name : "Regular polygon");
                Assets::RequestUpload();
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
            if (ImGui::Button("Create##Polygon"))
            {
                appLayer->AddMesh(pool, CreatePolygonMeshData<D>(meshes.PolyVertices), name[0] ? name : "Polygon");
                Assets::RequestUpload();
            }
        }
        else if (meshes.StatMeshToLoad == importedIndex)
            HandleLoadDialog(StatMeshTask, [&](const Dialog::Path &path) {
                const auto res = LoadStaticMeshDataFromObjFile<D>(path.string().c_str());
                VKIT_LOG_RESULT_ERROR(res);
                if (!res)
                    return;

                const StatMeshData<D> &data = res.GetValue();
                appLayer->AddMesh(pool, data, name[0] ? name : path.filename().string().c_str());
                Assets::RequestUpload();
            });
        if constexpr (D == D3)
        {
            if (meshes.StatMeshToLoad == 2)
            {
                const u32 mn = 8;
                const u32 mx = 256;
                ImGui::SliderScalar("Rings", ImGuiDataType_U32, &meshes.Rings, &mn, &mx);
                ImGui::SliderScalar("Sectors", ImGuiDataType_U32, &meshes.Sectors, &mn, &mx);
                if (ImGui::Button("Create##Sphere"))
                {
                    appLayer->AddMesh(pool, CreateSphereMeshData(meshes.Rings, meshes.Sectors),
                                      name[0] ? name : "Sphere");
                    Assets::RequestUpload();
                }
            }
            else if (meshes.StatMeshToLoad == 3)
            {
                const u32 mn = 8;
                const u32 mx = 256;
                ImGui::SliderScalar("Sides", ImGuiDataType_U32, &meshes.CylinderSides, &mn, &mx);
                if (ImGui::Button("Create##Cylinder"))
                {
                    appLayer->AddMesh(pool, CreateCylinderMeshData(meshes.CylinderSides), name[0] ? name : "Cylinder");
                    Assets::RequestUpload();
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
    else if constexpr (std::is_same_v<Vertex, ParaVertex<D3>>)
    {
        combo("Mesh", &meshes.ParaMeshToLoad, "Capsule\0Rounded box\0Torus\0");
        char name[256] = {0};
        ImGui::InputTextWithHint("Name", "Will default to mesh name", name, 256);
        const u32 mn = 3;
        const u32 mx = 128;
        ImGui::SliderScalar("Rings", ImGuiDataType_U32, &meshes.Rings, &mn, &mx);
        ImGui::SliderScalar("Sectors", ImGuiDataType_U32, &meshes.Sectors, &mn, &mx);
        if (ImGui::Button("Create##Parametric"))
        {
            if (meshes.ParaMeshToLoad == 0)
                appLayer->AddMesh(pool, CreateCapsuleMeshData(meshes.Rings, meshes.Sectors));
            else if (meshes.ParaMeshToLoad == 1)
                appLayer->AddMesh(pool, CreateRoundedBoxMeshData(meshes.Rings, meshes.Sectors));
            else if (meshes.ParaMeshToLoad == 2)
                appLayer->AddMesh(pool, CreateTorusMeshData(meshes.Rings, meshes.Sectors));

            Assets::RequestUpload();
        }
    }
}

template <typename Vertex> void SandboxWinLayer::RenderMesh(MeshId<Vertex> &mesh)
{
    constexpr Dimension D = Vertex::Dim;
    bool changed = false;

    ImGui::Text("Mesh handle: %s", TKit::Format("{:#010x}", mesh.Handle).c_str());
    if (ImGui::TreeNode("Vertex buffer"))
    {
        for (u32 i = 0; i < mesh.Data.Vertices.GetSize(); ++i)
        {
            Vertex &vx = mesh.Data.Vertices[i];
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
        Assets::UpdateMesh(mesh.Handle, mesh.Data);
        Assets::RequestUpload();
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
    ImGui::CheckboxFlags("Continuous update##Lattice", &lattice.Flags, SandboxFlag_ContextShouldUpdate);
    if (ImGui::TreeNode("Targets"))
    {
        auto &views = GetViews<D>();
        for (const ViewData<D> &vd : views.Views)
        {
            const ViewMask vbit = vd.View->GetViewBit();
            bool targets = vbit & lattice.Contexts[0]->GetViewMask();
            ImGui::PushID(vbit);
            if (ImGui::Checkbox("Target this view##Lattice", &targets))
            {
                if (targets)
                    for (Onyx::RenderContext<D> *ctx : lattice.Contexts)
                        ctx->AddTarget(vbit);
                else
                    for (Onyx::RenderContext<D> *ctx : lattice.Contexts)
                        ctx->RemoveTarget(vbit);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    bool updateShape = combo("Geometry##Lattice", &lattice.Geo, "Circle\0Static mesh\0Parametric mesh\0\0");
    const Geometry geo = lattice.Geo;

    SandboxAppLayer *appLayer = GetApplicationLayer<SandboxAppLayer>();
    if (geo == Geometry_Static)
        updateShape |= statMeshNameCombo<D>("Shape##Lattice", appLayer, &lattice.StatMesh);
    else if (geo == Geometry_Parametric)
        updateShape |= paraMeshNameCombo<D>("Shape##Lattice", appLayer, &lattice.ParaMesh);

    if (updateShape)
        lattice.Shape = appLayer->CreateShape<D>(geo, geo == Geometry_Static ? lattice.StatMesh : lattice.ParaMesh);

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
    if (event.Type != Event_Scrolled)
        return;

    const Window *win = GetWindow();
    RenderView<D> *rv = win->GetMouseRenderView<D>();
    if (!rv)
        return;

    const f32 factor = win->IsKeyPressed(Key_LeftShift) ? 0.05f : 0.005f;
    const f32v2 mpos = win->GetScreenMousePosition();
    if constexpr (D == D2)
        rv->ZoomScroll(mpos, factor * event.ScrollOffset[1]);
    else
        rv->ZoomScroll(f32v3{mpos, 0.f}, factor * event.ScrollOffset[1]);
}

template <Dimension D> RenderView<D> *SandboxWinLayer::AddView(Camera<D> *camera)
{
    auto &views = GetViews<D>();
    Window *win = GetWindow();
    const u32 size = views.Views.GetSize();
    ViewData<D> &view = views.Views.Append();
    view.View =
        win->CreateRenderView(camera, RenderViewFlag_Shadows | RenderViewFlag_PostProcess | RenderViewFlag_Outlines);
    view.View->ClearColor = Color{0.1f};
    view.Name = TKit::Format("View {}", size);

    return view.View;
}

template <Dimension D> Camera<D> *SandboxWinLayer::AddCamera()
{
    auto &cameras = GetCameras<D>();

    const u32 size = cameras.Cameras.GetSize();
    CameraData<D> &data = cameras.Cameras.Append();

    TKit::TierAllocator *tier = TKit::GetTier();
    data.Camera = tier->Create<Camera<D>>();
    data.Name = TKit::Format("Camera {}", size);

    if constexpr (D == D3)
    {
        data.Camera->View.Translation = f32v3{2.f, 0.75f, 2.f};
        data.Camera->View.Rotation = f32q{Math::Radians(f32v3{-15.f, 45.f, -4.f})};
    }
    return data.Camera;
}
} // namespace Onyx
