#include "engine.hpp"
#include "components.hpp"
#include "onyx/resources.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"

#define NAME_BUF_SIZE 64
#define DEFAULT_RESOLUTION u32v2{1920, 1080}

namespace Engine
{
struct Editor_Ids
{
    Onyx::LayoutId MainDockSpace = "__onyx_editor_Main_dockspace";
    Onyx::LayoutId EditorDockSpace = "__onyx_editor_Dockspace";

    Onyx::OverlayLabel Editor = "Editor";
    Onyx::OverlayLabel Hierarchy = "Hierarchy";
    Onyx::OverlayLabel Entity = "Entity";
    Onyx::OverlayLabel Console = "Console";
    Onyx::OverlayLabel AssetBrowser = "Asset browser";
    Onyx::OverlayLabel Rendering = "Rendering";
};

template <Dimension D> struct Editor_Camera
{
    TKit::TierString Name{};
    Onyx::Camera<D> *Handle;
    // lol
    u32 RefCount = 0;
};

template <Dimension D> struct Editor_RenderView
{
    TKit::TierString Name{};
    Onyx::RenderView<D> *Handle;
    // TODO(Isma): Need to store here as well what entity's camera will take over the view when play is hit
};

template <Dimension D> struct Editor_RenderContext
{
    TKit::TierString Name{};
    Onyx::RenderContext<D> *Handle;
};

struct Editor_Viewport
{
    TKit::TierString Name{};
    const Onyx::OverlayWindow *Window = nullptr;
    Onyx::RenderTexture *Target = nullptr;

    TKit::TierHive<Editor_RenderView<D2>> Views2{};
    TKit::TierHive<Editor_RenderView<D3>> Views3{};

    f32v2 Position{0.f};
    f32v2 Size{0.f};
    u32v2 Resolution{0};
    f32 Aspect = 0.f;
    bool Visible = true;

    template <Dimension D> TKit::TierHive<Editor_RenderView<D>> &GetViews()
    {
        if constexpr (D == D2)
            return Views2;
        else
            return Views3;
    }
};

struct Editor_Scene
{
    TKit::TierString Name{};
    TKit::Registry Registry{};

    Entity SelectedEntity = TKit::NullEntity;

    // TODO(Isma): If viewports are created/destroyed on play, we need to be able to restore that!
    TKit::TierHive<Editor_Viewport> Viewports{};

    TKit::TierArray<Editor_Camera<D2>> Cameras2{};
    TKit::TierArray<Editor_Camera<D3>> Cameras3{};

    TKit::TierHive<Editor_RenderContext<D2>> Contexts2{};
    TKit::TierHive<Editor_RenderContext<D3>> Contexts3{};

    template <Dimension D> TKit::TierArray<Editor_Camera<D>> &GetCameras()
    {
        if constexpr (D == D2)
            return Cameras2;
        else
            return Cameras3;
    }
    template <Dimension D> TKit::TierHive<Editor_RenderContext<D>> &GetContexts()
    {
        if constexpr (D == D2)
            return Contexts2;
        else
            return Contexts3;
    }
};

struct Editor_Data
{
    Onyx::Window *Window;
    Onyx::Overlay *Overlay;

    const Editor_Ids Labels{};
    TKit::TierHive<Editor_Scene> Scenes{};
    Scene ActiveScene = NullScene;

    Editor_Scene &GetActiveScene()
    {
        return Scenes[ActiveScene];
    }
};

static TKit::Storage<Editor_Data> s_Data{};

static TKit::StackString editor_CreateDefaultName(const TKit::StringView prefix, const u32 idx)
{
    return TKit::StackString::Format("{} {}", prefix, idx);
}

struct Utils_NameArray
{
    TKit::StackArray<TKit::StackString> Names{};
    TKit::StackArray<u32> Ids{};
};

template <typename T> static Utils_NameArray editor_CreateNameArray(const TKit::TierHive<T> &elements)
{
    Utils_NameArray array{};
    array.Names.Reserve(elements.GetSize());
    array.Ids.Reserve(elements.GetSize());

    elements.IterateByInsertionOrder([&](const u32 id, const T &element) {
        array.Names.Append(element.Name);
        array.Ids.Append(id);
    });
    return array;
}

template <typename T>
static TKit::StackArray<TKit::StackString> editor_CreateNameArray(const TKit::Span<const T> elements)
{
    TKit::StackArray<TKit::StackString> names{};
    names.Reserve(elements.GetSize());
    for (const T &elm : elements)
        names.Append(elm.Name);
    return names;
}

template <Dimension D> static RenderView viewport_CreateRenderView(Editor_Viewport &viewport, Editor_Camera<D> &cam)
{
    ++cam.RefCount;

    TKit::TierHive<Editor_RenderView<D>> &rvs = viewport.GetViews<D>();
    const RenderView rv = rvs.Insert();

    Editor_RenderView<D> &rview = rvs[rv];
    rview.Name = editor_CreateDefaultName("View", rv);
    rview.Handle = viewport.Target->CreateRenderView<D>(cam.Handle, Onyx::RenderViewFlag_NormalizedCoordinates);

    return rv;
}

template <Dimension D>
static void viewport_DestroyRenderView(Editor_Viewport &viewport, const RenderView rv, Editor_Camera<D> &cam)
{
    TKit::TierHive<Editor_RenderView<D>> &rvs = viewport.GetViews<D>();
    Editor_RenderView<D> &rview = rvs[rv];

    TKIT_ASSERT(cam.Handle == rview.Handle->GetCamera(),
                "[ONYX][EDITOR] When destroying a render view, the passed camera must be attached to the view");
    --cam.RefCount;
    viewport.Target->DestroyRenderView(rview.Handle);
    rvs.Remove(rv);
}

template <Dimension D>
static void viewport_DestroyRenderView(Editor_Scene &scene, Editor_Viewport &viewport, RenderView rv)
{
    Editor_RenderView<D> &rview = viewport.GetViews<D>()[rv];
    for (Editor_Camera<D> &cam : scene.GetCameras<D>())
        if (cam.Handle == rview.Handle->GetCamera())
        {
            viewport_DestroyRenderView(viewport, rv, cam);
            return;
        }
    TKIT_FATAL("[ONYX][EDITOR] Found no camera related to render view {} of viewport {} to destroy", rview.Name,
               viewport.Name);
}

Scene Scene_Create()
{
    const Scene sc = s_Data->Scenes.Insert();
    Editor_Scene &scene = s_Data->Scenes[sc];
    scene.Name = editor_CreateDefaultName("Scene", sc);
    scene.Registry.RegisterComponents(AllComponents{});
    return sc;
}

void Scene_Destroy(const Scene sc)
{
    Editor_Scene &scene = s_Data->Scenes[sc];
    const TKit::StackArray<Viewport> vps = scene.Viewports.GetValidIds();

    for (const Viewport vp : vps)
        Scene_DestroyViewport(sc, vp);

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const Editor_Camera<D2> &cam : scene.Cameras2)
        tier->Destroy(cam.Handle);
    for (const Editor_Camera<D3> &cam : scene.Cameras3)
        tier->Destroy(cam.Handle);

    s_Data->Scenes.Remove(sc);
}

Viewport Scene_CreateViewport(const Scene sc, const u32v2 &resolution)
{
    Editor_Scene &scene = s_Data->Scenes[sc];
    const Viewport vp = scene.Viewports.Insert();
    Editor_Viewport &viewport = scene.Viewports[vp];

    viewport.Target = Onyx::CreateRenderTexture(resolution);
    viewport.Name = editor_CreateDefaultName("Viewport", vp);
    viewport.Position = 0.f;
    viewport.Size = 1.f;
    viewport.Resolution = resolution;
    viewport.Aspect = f32(resolution[0]) / f32(resolution[1]);

    return vp;
}

void Scene_DestroyViewport(const Scene sc, const Viewport vp)
{
    Editor_Scene &scene = s_Data->Scenes[sc];
    Editor_Viewport &viewport = scene.Viewports[sc];

    // we do it "manually" cause we want to remove the refcounts of the cameras
    const TKit::StackArray<RenderView> rvs2 = viewport.Views2.GetValidIds();
    for (const RenderView rv : rvs2)
        viewport_DestroyRenderView<D2>(scene, viewport, rv);

    const TKit::StackArray<RenderView> rvs3 = viewport.Views3.GetValidIds();
    for (const RenderView rv : rvs3)
        viewport_DestroyRenderView<D3>(scene, viewport, rv);

    Onyx::DestroyRenderTexture(viewport.Target);
    scene.Viewports.Remove(vp);
}

template <Dimension D> RenderContext Scene_CreateRenderContext(const Scene sc)
{
    Editor_Scene &scene = s_Data->Scenes[sc];
    TKit::TierHive<Editor_RenderContext<D>> &rcs = scene.GetContexts<D>();

    const RenderContext rc = rcs.Insert();
    Editor_RenderContext<D> &rctx = rcs[rc];
    rctx.Name = editor_CreateDefaultName("Context", rc);
    rctx.Handle = Onyx::CreateRenderContext<D>();

    return rc;
}
template <Dimension D> void Scene_DestroyRenderContext(const Scene sc, const RenderContext rc)
{
    Editor_Scene &scene = s_Data->Scenes[sc];
    TKit::TierHive<Editor_RenderContext<D>> &rcs = scene.GetContexts<D>();

    Editor_RenderContext<D> &rctx = rcs[rc];
    Onyx::DestroyRenderContext(rctx.Handle);
    rcs.Remove(rc);
}

void Scene_FlushContexts(const Scene sc)
{
    const Editor_Scene &scene = s_Data->Scenes[sc];
    for (const Editor_RenderContext<D2> &ctx : scene.Contexts2)
        ctx.Handle->Flush();
    for (const Editor_RenderContext<D3> &ctx : scene.Contexts3)
        ctx.Handle->Flush();
}

TKit::Registry &Scene_GetRegistry(const Scene sc)
{
    return s_Data->Scenes[sc].Registry;
}

// template <Dimension D> RenderView Viewport_CreateRenderView(const Viewport vp)
// {
// }
// template <Dimension D> void Viewport_DestroyRenderView(const Viewport vp, const RenderView rv)
// {
// }

template <Dimension D> static Editor_Camera<D> &scene_CreateEditorCamera(Editor_Scene &scene)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    TKit::TierArray<Editor_Camera<D>> &cams = scene.GetCameras<D>();
    return cams.Append(editor_CreateDefaultName("Camera", cams.GetSize()), tier->Create<Onyx::Camera<D>>(), 0);
}

template <Dimension D> static void scene_DestroyEditorCamera(Editor_Scene &scene, const u32 camIdx)
{
    TKit::TierArray<Editor_Camera<D>> &cams = scene.GetCameras<D>();

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(cams[camIdx].Handle);

    cams.RemoveOrdered(cams.begin() + camIdx);
}

static Onyx::Window *editor_CreateWindow()
{
#ifdef TKIT_ENABLE_ENSURE
    const char *title = "Onyx editor - " ONYX_VERSION " - [DEBUG]";
#else
    const char *title = "Onyx editor - " ONYX_VERSION;
#endif
    return Onyx::OpenWindow({.Window = {.Title = title}});
}

static Onyx::Overlay *editor_CreateOverlay(Onyx::Window *win)
{
    Onyx::Overlay *ov = win->CreateOverlay({.Flags = Onyx::OverlayFlag_Docking | Onyx::OverlayFlag_AutoSerialize});

    // TODO(Isma): Change this with a project-specific path!
    if (!ov->Deserialize("."))
    {
        const Onyx::LayoutId mainViewportId = 0u;
        const Editor_Ids &idData = s_Data->Labels;
        ov->DeclareWindow(idData.Editor.Id);
        ov->DeclareDockSpace(idData.EditorDockSpace.Id, idData.Editor.Id);

        const Onyx::OverlayDockNode *mainTree = Onyx::DockTabBar(idData.Editor.Id);
        const Onyx::OverlayDockNode *editorTree = Onyx::DockSplit(
            Onyx::LayoutAxis_Vertical, 0.15f,
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f, Onyx::DockTabBar(idData.Hierarchy.Id),
                            Onyx::DockTabBar(idData.Entity.Id)),
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f,
                            Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.65f, Onyx::DockTabBar(mainViewportId),
                                            Onyx::DockTabBar(idData.Rendering.Id)),
                            Onyx::DockTabBar({idData.Console.Id, idData.AssetBrowser.Id})));

        ov->ApplyDockTree(idData.MainDockSpace, mainTree);
        ov->ApplyDockTree(idData.EditorDockSpace, editorTree);
    }

    return ov;
}

void Initialize()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    s_Data.Construct();

    s_Data->Window = editor_CreateWindow();
    s_Data->Overlay = editor_CreateOverlay(s_Data->Window);

    const Scene sc = Scene_Create();
    s_Data->ActiveScene = sc;
    Scene_CreateViewport(sc, DEFAULT_RESOLUTION);
}

static void editorWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;

    ov->PushStyleVar(Onyx::OverlayStyle_ContentAreaPadding, 0.f);
    const bool opened = ov->BeginWindow(idData.Editor);
    ov->PopStyleVar();
    if (opened)
    {
        ov->DockSpace(idData.EditorDockSpace, Onyx::OverlayDockNodeFlag_CanBeEmpty, Onyx::OverlayWindowFlag_ChildGrow);
        ov->EndWindow();
    }
}
static void viewportWindow_Draw()
{
    TKit::TierHive<Editor_Viewport> &vps = s_Data->GetActiveScene().Viewports;
    if (vps.IsEmpty())
        return;

    Onyx::Overlay *ov = s_Data->Overlay;
    const auto drawViewportWindow = [&](const Viewport vp) {
        Editor_Viewport &viewport = vps[vp];
        if (ov->BeginWindow({vp, viewport.Name}, &viewport.Visible, Onyx::OverlayWindowFlag_MenuBar))
        {
            viewport.Window = ov->GetActiveWindow();

            const Onyx::LayoutId panelId = &viewport;
            const Onyx::LayoutId imgId = &viewport.Resolution;
            const Onyx::LayoutElementQueryInfo *vpParentElm = ov->QueryElement(panelId);
            const Onyx::LayoutElementQueryInfo *vpElm = ov->QueryElement(imgId);

            ov->BeginPanel(panelId, Onyx::LayoutPanelParameters{.Alignment = Onyx::Alignment_Center,
                                                                .Sizing = Onyx::LayoutSizing::Grow()});
            if (vpElm)
                viewport.Position = vpElm->Position;

            if (vpParentElm)
            {
                const f32v2 &size = vpParentElm->Size;
                const f32 aspect = size[0] / size[1];

                f32 w;
                f32 h;
                if (aspect < viewport.Aspect)
                {
                    w = vpParentElm->Size[0];
                    h = w / viewport.Aspect;
                }
                else
                {
                    h = vpParentElm->Size[1];
                    w = h * viewport.Aspect;
                }

                viewport.Size = f32v2{w, h};
                ov->Image(imgId, *viewport.Target, viewport.Size);
            }
            else
                ov->Image(imgId, *viewport.Target, Onyx::LayoutSizing::Grow());
            ov->EndPanel();

            if (ov->BeginMenuBar())
            {
                if (ov->BeginMenu("Resolution"))
                {
                    ov->BeginScroll("Scroll", 200.f,
                                    Onyx::OverlayScrollFlag_NoBackground | Onyx::OverlayScrollFlag_FlexWidth);
                    const auto addResolution = [&](const char *name, const u32 w, const u32 h) {
                        const u32v2 r = u32v2{w, h};
                        if (ov->MenuItem(name, viewport.Resolution == r))
                        {
                            viewport.Target->Resize(r);
                            viewport.Resolution = r;
                            viewport.Aspect = f32(w) / f32(h);
                        }
                    };

                    addResolution("640x480 (4:3)", 640, 480);
                    addResolution("800x600 (4:3)", 800, 600);
                    addResolution("1024x768 (4:3)", 1024, 768);
                    addResolution("1280x720 (16:9)", 1280, 720);
                    addResolution("1280x800 (16:10)", 1280, 800);
                    addResolution("1366x768 (~16:9)", 1366, 768);
                    addResolution("1440x900 (16:10)", 1440, 900);
                    addResolution("1600x900 (16:9)", 1600, 900);
                    addResolution("1680x1050 (16:10)", 1680, 1050);
                    addResolution("1920x1080 (16:9)", 1920, 1080);
                    addResolution("1920x1200 (16:10)", 1920, 1200);
                    addResolution("2560x1080 (21:9)", 2560, 1080);
                    addResolution("2560x1440 (16:9)", 2560, 1440);
                    addResolution("2560x1600 (16:10)", 2560, 1600);
                    addResolution("3440x1440 (21:9)", 3440, 1440);
                    addResolution("3840x1600 (21:9)", 3840, 1600);
                    addResolution("3840x2160 (16:9)", 3840, 2160);
                    addResolution("5120x1440 (32:9)", 5120, 1440);
                    addResolution("5120x2160 (21:9)", 5120, 2160);
                    addResolution("5120x2880 (16:9)", 5120, 2880);
                    addResolution("7680x4320 (16:9)", 7680, 4320);
                    ov->EndScroll();
                    ov->EndMenu();
                }
                ov->EndMenuBar();
            }

            ov->EndWindow();
        }
    };
    ov->PushStyleVar(Onyx::OverlayStyle_ContentAreaPadding, 0.f);
    for (const Viewport vp : vps.GetValidIds())
        drawViewportWindow(vp);
    ov->PopStyleVar();
}
static void hierarchyWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Hierarchy))
    {
        Editor_Scene &scene = s_Data->GetActiveScene();
        TKit::Registry &r = scene.Registry;
        if (ov->Button("Add entity", Onyx::OverlayButtonFlag_SpanFullWidth))
        {
            const Entity e = r.CreateEntity();
            r.AddComponent<NameComponent>(e, editor_CreateDefaultName("Entity", e));
        }

        r.IterateEntitiesByInsertionOrder([&](const Entity e) {
            const NameComponent *nc = scene.Registry.GetComponent<NameComponent>(e);
            TKIT_ASSERT(nc, "[ONYX][EDITOR] All editor entities must have a name, but entity {} does not", e);
            if (ov->Selectable({e, nc->Name}, e == scene.SelectedEntity))
                scene.SelectedEntity = e;
        });

        ov->EndWindow();
    }
}
template <typename C, typename... Args>
static void entityWindow_ChooseComponent(const Entity e, const char *name, TKit::Registry &registry, Args &&...args)
{
    if (registry.HasComponent<C>(e))
        return;

    Onyx::Overlay *ov = s_Data->Overlay;
    if (ov->Button(name, Onyx::OverlayButtonFlag_SpanFullWidth))
        registry.AddComponent<C>(e, std::forward<Args>(args)...);
}
template <template <Dimension> typename C>
static void entityWindow_ChooseComponent(const Entity e, const char *name, TKit::Registry &registry,
                                         const C<D2> &c2 = {}, const C<D3> &c3 = {})
{
    const bool has2 = registry.HasComponent<C<D2>>(e);
    const bool has3 = registry.HasComponent<C<D3>>(e);
    if (has2 && has3)
        return;

    Onyx::Overlay *ov = s_Data->Overlay;
    ov->BeginPanel(Onyx::LayoutPanelParameters{.Direction = Onyx::LayoutDirection_LeftToRight,
                                               .Alignment = ov->TopLeft,
                                               .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                               .ChildGap = ov->GetStyle()[Onyx::OverlayStyle_ChildGap]});

    ov->TextRaw(name);

    ov->Panel(Onyx::LayoutPanelParameters{.Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Grow()}});

    ov->PushId(name);

    ov->BeginDisabled(has2);
    if (ov->Button("2D"))
        registry.AddComponent<C<D2>>(e, c2);
    ov->EndDisabled();
    if (has2)
        ov->SetItemTooltipRaw("This entity already has this component", Onyx::OverlayFocusFlag_NormalDelay);

    ov->BeginDisabled(has3);
    if (ov->Button("3D"))
        registry.AddComponent<C<D3>>(e, c3);
    ov->EndDisabled();
    if (has3)
        ov->SetItemTooltipRaw("This entity already has this component", Onyx::OverlayFocusFlag_NormalDelay);

    ov->PopId();

    ov->EndPanel();
}
static void entityWindow_DisplayComponents(const Entity e, const TKit::Registry &registry)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    NameComponent *name = registry.GetComponent<NameComponent>(e);
    TKIT_ASSERT(name, "[ONYX][EDITOR] All entities in the editor must have a name");

    ov->InputText("Name", &name->Name, NAME_BUF_SIZE);
}
template <Dimension D> static void entityWindow_DisplayComponents(const Entity e, const TKit::Registry &registry)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    TransformComponent<D> *transform = registry.GetComponent<TransformComponent<D>>(e);
    ov->PushId(u32(D));
    if (transform)
    {
        Onyx::Transform<D> &t = transform->Transform;
        const auto resetPopup = [&](const char *title, auto &field, auto &&value) {
            if (ov->BeginPopupContextItem(title,
                                          Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_AutoResize))
            {
                if (ov->Button("Reset"))
                {
                    field = value;
                    ov->CloseCurrentPopup();
                }
                ov->EndPopup();
            }
        };

        if constexpr (D == D2)
            ov->HorizontalSeparator("Transform 2D");
        else
            ov->HorizontalSeparator("Transform 3D");

        constexpr f32 speed = 0.03f;
        ov->HorizontalDrag("Translation", &t.Translation, speed);
        resetPopup("Reset##Translation", t.Translation, f32v<D>{0.f});

        ov->HorizontalDrag("Scale", &t.Scale, speed);
        resetPopup("Reset##Scale", t.Scale, f32v<D>{1.f});

        if constexpr (D == D2)
        {
            f32 degs = Math::Degrees(t.Rotation);
            if (ov->HorizontalDrag("Rotation", &degs, speed))
                t.Rotation = Math::Radians(degs);
        }
        else
        {
            f32q &q = t.Rotation;
            f32v3 euler = Math::Degrees(Math::ToEulerAngles(q));
            if (ov->HorizontalDrag("Rotation", &euler, speed))
                q = f32q::FromEulerAngles(Math::Radians(euler));
        }
        resetPopup("Reset##Rotation", t.Rotation, Onyx::RotType<D>::Identity);
    }

    StaticMeshComponent<D> *statMesh = registry.GetComponent<StaticMeshComponent<D>>(e);
    if (statMesh)
    {
        if constexpr (D == D2)
            ov->HorizontalSeparator("Static mesh 2D");
        else
            ov->HorizontalSeparator("Static mesh 3D");

        const Onyx::DefaultResources &defRes = Onyx::Resources::GetDefaultResources();
        TKit::StackArray<Onyx::Resource> defShapes{};

        defShapes.Reserve(5);
        defShapes.Append(defRes.GetTriangle<D>());
        defShapes.Append(defRes.GetQuad<D>());
        if constexpr (D == D2)
            ov->DropDown("Shape", &statMesh->Index, "Triangle#Quad");
        else
        {
            defShapes.Append(defRes.Box);
            defShapes.Append(defRes.Sphere);
            defShapes.Append(defRes.Cylinder);
            ov->DropDown("Shape", &statMesh->Index, "Triangle#Quad#Box#Sphere#Cylinder");
        }
        statMesh->Mesh = defShapes[statMesh->Index];
        ov->ColorEditor("Base color", &statMesh->Color);
    }

    RenderContextComponent<D> *rc = registry.GetComponent<RenderContextComponent<D>>(e);
    if (rc)
    {
        if constexpr (D == D2)
            ov->HorizontalSeparator("Render context 2D");
        else
            ov->HorizontalSeparator("Render context 3D");

        ov->TextRaw("Placeholder: currently, there is only one context available for rendering");
    }
    ov->PopId();
}

static void entityWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    Editor_Scene &scene = s_Data->GetActiveScene();

    const Entity e = scene.SelectedEntity;
    if (e != NullEntity && ov->BeginWindow(idData.Entity))
    {
        TKit::Registry &r = scene.Registry;

        const auto hasAllComponents = [&] { return r.HasAllComponents(AllComponents{}, e); };

        if (ov->BeginPopup("Components"))
        {
            const Onyx::DefaultResources &defRes = Onyx::Resources::GetDefaultResources();

            entityWindow_ChooseComponent<TransformComponent>(e, "Transform", r);
            entityWindow_ChooseComponent<StaticMeshComponent>(e, "Static mesh", r, {1, defRes.Quad2},
                                                              {1, defRes.Quad3});
            entityWindow_ChooseComponent<RenderContextComponent>(e, "Render context", r);

            if (hasAllComponents())
                ov->CloseCurrentPopup();
            ov->EndPopup();
        }

        const bool hasAll = hasAllComponents();
        ov->BeginDisabled(hasAll);
        if (ov->Button("Add component", Onyx::OverlayButtonFlag_SpanFullWidth))
            ov->OpenPopup("Components");
        ov->EndDisabled();
        if (hasAll)
            ov->SetItemTooltipRaw("All possible components have already been added to this entity",
                                  Onyx::OverlayFocusFlag_NormalDelay);

        entityWindow_DisplayComponents(e, r);
        entityWindow_DisplayComponents<D2>(e, r);
        entityWindow_DisplayComponents<D3>(e, r);

        ov->EndWindow();
    }
}
static void assetBrowserWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.AssetBrowser))
    {
        ov->EndWindow();
    }
}
static void consoleWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Console))
    {
        ov->EndWindow();
    }
}

struct Utils_ListBox
{
    TKit::StringView Title;
    u32 *Selected;
    const TKit::StackArray<TKit::StackString> *Labels;

    std::function<void()> OnAdd = nullptr;
    std::function<void()> OnRemove = nullptr;

    TKit::StringView CannotAddTooltip{};
    TKit::StringView CannotRemoveTooltip{};
    bool CanAdd = true;
    bool CanRemove = true;
};

static bool editor_ListBox(const Utils_ListBox &params)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    ov->PushDirection(Onyx::LayoutDirection_LeftToRight);

    u32 *selected = params.Selected;
    const TKit::StackArray<TKit::StackString> &labels = *params.Labels;

    ov->PushDirection(Onyx::LayoutDirection_TopToBottom, Onyx::LayoutSizing::Fit());
    if (params.OnAdd)
    {
        const bool canAdd = params.CanAdd;
        ov->BeginDisabled(!canAdd);
        if (ov->Button("+"))
            params.OnAdd();
        ov->EndDisabled();

        if (!canAdd && !params.CannotAddTooltip.IsEmpty())
            ov->SetItemTooltipRaw(params.CannotAddTooltip, Onyx::OverlayFocusFlag_NormalDelay);
    }

    if (params.OnRemove)
    {
        const bool exceeds = *selected >= labels.GetSize();
        ov->BeginDisabled(!params.CanRemove || exceeds);
        if (ov->Button("-", Onyx::OverlayButtonFlag_SpanFullWidth))
            params.OnRemove();
        ov->EndDisabled();

        if (exceeds)
            ov->SetItemTooltipRaw("Select an item to remove", Onyx::OverlayFocusFlag_NormalDelay);
        else if (!params.CanRemove && !params.CannotRemoveTooltip.IsEmpty())
            ov->SetItemTooltipRaw(params.CannotRemoveTooltip, Onyx::OverlayFocusFlag_NormalDelay);
    }

    ov->PopDirection();

    const bool result =
        ov->ListBox<TKit::StackString>(params.Title, selected, labels, Onyx::OverlaySelectableFlag_ListBoxUnselect);
    ov->PopDirection();
    return result;
}

template <Dimension D> static void renderingWindow_DisplayView(Editor_RenderView<D> &view)
{
    Onyx::Overlay *ov = s_Data->Overlay;

    ov->HorizontalSeparator(view.Name);
    ov->InputText("Name", &view.Name, NAME_BUF_SIZE);

    Onyx::RenderView<D> *rv = view.Handle;
    Editor_Scene &scene = s_Data->GetActiveScene();

    TKit::TierArray<Editor_Camera<D>> &cams = scene.GetCameras<D>();
    const TKit::StackArray<TKit::StackString> camLabels = editor_CreateNameArray<Editor_Camera<D>>(cams);

    u32 selected = TKIT_U32_MAX;
    for (u32 i = 0; i < cams.GetSize(); ++i)
        if (cams[i].Handle == rv->GetCamera())
        {
            selected = i;
            break;
        }
    TKIT_ASSERT(selected != TKIT_U32_MAX,
                "[ONYX][EDITOR] Could not find the camera the render view {} is associated with", view.Name);

    const u32 prev = selected;
    if (ov->DropDown<TKit::StackString>("Camera", &selected, camLabels) && selected != prev)
    {
        --cams[prev].RefCount;
        ++cams[selected].RefCount;
        rv->SetCamera(cams[selected].Handle);
    }

    ov->ColorEditor("Background color", &rv->ClearColor);

    ov->HorizontalSeparator("Viewport");
    Onyx::RenderViewFlags flags = rv->GetFlags();

    const bool nv = flags & Onyx::RenderViewFlag_NormalizedViewportCoordinates;
    const auto getVp = nv ? &Onyx::RenderView<D>::GetNormalizedViewport : &Onyx::RenderView<D>::GetAbsoluteViewport;
    const auto setVp = nv ? &Onyx::RenderView<D>::SetNormalizedViewport : &Onyx::RenderView<D>::SetAbsoluteViewport;

    const bool ns = flags & Onyx::RenderViewFlag_NormalizedScissorCoordinates;
    const auto getSc = ns ? &Onyx::RenderView<D>::GetNormalizedScissor : &Onyx::RenderView<D>::GetAbsoluteScissor;
    const auto setSc = ns ? &Onyx::RenderView<D>::SetNormalizedScissor : &Onyx::RenderView<D>::SetAbsoluteScissor;

    const f32 aspeed = 1.f;
    const f32 nspeed = 0.001f;
    const f32 vspeed = nv ? nspeed : aspeed;
    const f32 sspeed = ns ? nspeed : aspeed;

    Onyx::Viewport viewport = (rv->*getVp)();
    bool changed = ov->HorizontalDrag("Position##Viewport", &viewport.Position, vspeed);
    changed |= ov->HorizontalDrag("Extent##Viewport", &viewport.Extent, vspeed);
    if (changed)
        (rv->*setVp)(viewport);

    ov->HorizontalSeparator("Scissor");
    Onyx::Scissor sc = (rv->*getSc)();
    changed = ov->HorizontalDrag("Position##Scissor", &sc.Position, sspeed);
    changed |= ov->HorizontalDrag("Extent##Scissor", &sc.Extent, sspeed);
    if (changed)
        (rv->*setSc)(sc);

    changed = ov->CheckBoxFlags("Normalized coordinates", &flags, Onyx::RenderViewFlag_NormalizedCoordinates);
    changed |= ov->CheckBoxFlags("Shadows", &flags, Onyx::RenderViewFlag_Shadows);

    if (ov->CheckBoxFlags("Post-process", &flags, Onyx::RenderViewFlag_PostProcess))
    {
        if (!(flags & Onyx::RenderViewFlag_PostProcess))
            flags &= ~Onyx::RenderViewFlag_Outlines;

        changed = true;
    }

    const bool mustDisable = !(flags & Onyx::RenderViewFlag_PostProcess);
    ov->BeginDisabled(mustDisable);
    changed |= ov->CheckBoxFlags("Outlines", &flags, Onyx::RenderViewFlag_Outlines);
    ov->EndDisabled();
    if (mustDisable)
        ov->SetItemTooltipRaw("Outlines can only be enabled with post-processing", Onyx::OverlayFocusFlag_NormalDelay);

    changed |= ov->CheckBoxFlags("Transparency", &flags, Onyx::RenderViewFlag_Transparency);
    changed |= ov->CheckBoxFlags("Hidden", &flags, Onyx::RenderViewFlag_Hidden);

    if (changed)
        rv->SetFlags(flags);
}
template <Dimension D> static void renderingWindow_DisplayViews(const char *name, Editor_Viewport &viewport)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    if (ov->PushTree(name, Onyx::OverlayTreeFlag_DrawLines))
    {
        TKit::TierHive<Editor_RenderView<D>> &views = viewport.GetViews<D>();

        static u32 selected = TKIT_U32_MAX;
        const Utils_NameArray labels = editor_CreateNameArray(views);

        Editor_Scene &scene = s_Data->GetActiveScene();
        TKit::TierArray<Editor_Camera<D>> &cams = scene.GetCameras<D>();
        if (ov->BeginPopup("Choose camera"))
        {
            u32 camera = TKIT_U32_MAX;

            for (u32 i = 0; i < cams.GetSize(); ++i)
                if (ov->Button({i, cams[i].Name}, Onyx::OverlayButtonFlag_SpanFullWidth))
                    camera = i;

            if (camera < cams.GetSize())
            {
                viewport_CreateRenderView(viewport, cams[camera]);
                ov->CloseCurrentPopup();
            }

            ov->EndPopup();
        }

        Utils_ListBox lb{"Views", &selected, &labels.Names};
        lb.OnAdd = [&] { ov->OpenPopup("Choose camera"); };
        lb.OnRemove = [&] { viewport_DestroyRenderView<D>(scene, viewport, labels.Ids[selected]); };

        lb.CanAdd = !cams.IsEmpty();
        lb.CannotAddTooltip =
            D == D2 ? "Add a 2D camera to be able to add a view" : "Add a 3D camera to be able to add a view";

        editor_ListBox(lb);

        const RenderView selectedRv = labels.Ids[selected];
        if (views.Contains(selectedRv))
            renderingWindow_DisplayView(views[selectedRv]);
        ov->PopTree();
    }
}

template <Dimension D> static void renderingWindow_DisplayCamera(Editor_Camera<D> &camera)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const char *elements = D == D2 ? "Orthographic#Viewport" : "Orthographic#Viewport#Perspective";

    ov->HorizontalSeparator(camera.Name);
    ov->InputText("Name", &camera.Name, NAME_BUF_SIZE);

    Onyx::Camera<D> *cam = camera.Handle;
    ov->DropDown("Mode", &cam->Mode, elements);
    if (cam->Mode == Onyx::CameraMode_Orthographic)
    {
        Onyx::OrthographicParameters<D> &params = cam->OrthoParameters;
        ov->HorizontalDrag("Size", &params.Size, 0.05f, 0.f, TKIT_F32_MAX);
        if constexpr (D == D3)
        {
            ov->HorizontalDrag("Near", &params.Near, 0.01f, 0.f, TKIT_F32_MAX);
            ov->HorizontalDrag("Far", &params.Far, 0.1f, 0.f, TKIT_F32_MAX);
        }
    }
    if constexpr (D == D3)
        if (cam->Mode == Onyx::CameraMode_Perspective)
        {
            Onyx::PerspectiveParameters &params = cam->PerspParameters;
            f32 fov = Math::Degrees(params.FieldOfView);
            if (ov->HorizontalDrag("Field of view", &fov, 0.1f, 0.f, TKIT_F32_MAX))
                params.FieldOfView = Math::Radians(fov);

            ov->HorizontalDrag("Near", &params.Near, 0.1f, 0.001f, params.Far);
            ov->HorizontalDrag("Far", &params.Far, 0.1f, params.Near, TKIT_F32_MAX);
        }
}
template <Dimension D> static void renderingWindow_DisplayCameras(const char *name)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    if (ov->PushTree(name, Onyx::OverlayTreeFlag_DrawLines))
    {
        Editor_Scene &scene = s_Data->GetActiveScene();

        TKit::TierArray<Editor_Camera<D>> &cams = scene.GetCameras<D>();
        const TKit::StackArray<TKit::StackString> labels = editor_CreateNameArray<Editor_Camera<D>>(cams);
        static u32 selected = TKIT_U32_MAX;

        Utils_ListBox lb{"Cameras", &selected, &labels};
        lb.OnAdd = [&] { scene_CreateEditorCamera<D>(scene); };
        lb.OnRemove = [&] { scene_DestroyEditorCamera<D>(scene, selected); };
        lb.CanRemove = selected < cams.GetSize() && cams[selected].RefCount == 0;
        lb.CannotRemoveTooltip =
            "A view is currently using this camera. Remove the view or change its camera to delete it";
        editor_ListBox(lb);

        if (selected < cams.GetSize())
            renderingWindow_DisplayCamera(cams[selected]);
        ov->PopTree();
    }
}

static void renderingWindow_DisplayViewport(Editor_Viewport &viewport)
{
    Onyx::Overlay *ov = s_Data->Overlay;
    ov->HorizontalSeparator(viewport.Name);
    ov->InputText("Name", &viewport.Name, NAME_BUF_SIZE);
    ov->Text("Resolution: {}x{}", viewport.Resolution[0], viewport.Resolution[1]);
    ov->CheckBox("Visible", &viewport.Visible);

    renderingWindow_DisplayViews<D2>("2D Views", viewport);
    renderingWindow_DisplayViews<D3>("3D Views", viewport);
}

static void renderingWindow_DisplayViewports()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    if (ov->PushTree("Viewports", Onyx::OverlayTreeFlag_DrawLines))
    {
        Editor_Scene &scene = s_Data->GetActiveScene();

        const Utils_NameArray labels = editor_CreateNameArray(scene.Viewports);
        static u32 selected = TKIT_U32_MAX;

        Utils_ListBox lb{"Viewports", &selected, &labels.Names};
        lb.OnAdd = [&] { Scene_CreateViewport(s_Data->ActiveScene, DEFAULT_RESOLUTION); };
        lb.OnRemove = [&] { Scene_DestroyViewport(s_Data->ActiveScene, labels.Ids[selected]); };
        editor_ListBox(lb);

        const Viewport selectedVp = labels.Ids[selected];
        if (scene.Viewports.Contains(selectedVp))
            renderingWindow_DisplayViewport(scene.Viewports[selectedVp]);
        ov->PopTree();
    }
}

static void renderingWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Rendering))
    {
        renderingWindow_DisplayViewports();
        renderingWindow_DisplayCameras<D2>("2D Editor cameras");
        renderingWindow_DisplayCameras<D3>("3D Editor cameras");
        ov->EndWindow();
    }
}

template <Dimension D>
static Onyx::RenderView<D> *editor_GetHoveredView(const Editor_Viewport &viewport, f32v2 *outMpos = nullptr)
{
    Onyx::Window *win = s_Data->Window;
    const Onyx::RenderTexture *rt = viewport.Target;

    const auto views = rt->GetSortedViews<D>();
    const f32v2 vpScreenPos =
        s_Data->Overlay->GetMainNativeWindow()->ToLocalScreen(viewport.Position + f32v2{0.f, viewport.Size[1]});

    const f32v2 ampos = win->GetAbsoluteMousePosition() - vpScreenPos;
    const f32v2 nmpos = ampos / viewport.Size;
    for (Onyx::RenderView<D> *rv : views)
    {
        const bool normalized = rv->GetFlags() & Onyx::RenderViewFlag_NormalizedViewportCoordinates;
        const f32v2 mpos = normalized ? nmpos : ampos;
        if (rv->IsWithinViewport(mpos))
        {
            if (outMpos)
                *outMpos = mpos;
            return rv;
        }
    }
    return nullptr;
}

template <Dimension D> static void editor_ApplyCameraMovement()
{
    Editor_Scene &scene = s_Data->GetActiveScene();
    for (const Editor_Viewport &viewport : scene.Viewports)
        if (viewport.Window && viewport.Window->IsHovered())
        {
            Onyx::RenderView<D> *rv = editor_GetHoveredView<D>(viewport);
            if (rv)
            {
                Onyx::Window *win = s_Data->Window;
                const TKit::Timespan dt = Onyx::GetDeltaTime(win);
                win->ControlCamera(dt, rv->GetCamera());
                return;
            }
        }
}

template <Dimension D> static void editor_ApplyZoom(const f32 scroll)
{
    Editor_Scene &scene = s_Data->GetActiveScene();
    for (const Editor_Viewport &viewport : scene.Viewports)
        if (viewport.Window && viewport.Window->IsHovered())
        {
            f32v2 mpos;
            Onyx::RenderView<D> *rv = editor_GetHoveredView<D>(viewport, &mpos);
            if (rv)
            {
                Onyx::Window *win = s_Data->Window;
                const f32 factor = win->IsKeyPressed(Onyx::Key_LeftShift) ? 0.05f : 0.005f;
                if constexpr (D == D2)
                    rv->ZoomScroll(mpos, factor * scroll);
                else
                    rv->ZoomScroll(f32v3{mpos, 0.5f}, factor * scroll);
                return;
            }
        }
}

static void editor_ControlCamera()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    Onyx::Window *win = s_Data->Window;
    if (!ov->WantCaptureKeyboard())
    {
        editor_ApplyCameraMovement<D2>();
        editor_ApplyCameraMovement<D3>();
    }

    if (!ov->WantCaptureScroll())
        for (const Onyx::Event &ev : win->GetNewEvents())
            if (ev.Type == Onyx::Event_Scrolled)
            {
                const f32 scroll = ev.ScrollOffset[1];
                editor_ApplyZoom<D2>(scroll);
                editor_ApplyZoom<D3>(scroll);
                break;
            }
}

void Run()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const Editor_Ids &idData = s_Data->Labels;
    while (Onyx::Running())
    {
        ov->FullScreenDockSpace(idData.MainDockSpace,
                                Onyx::OverlayDockNodeFlag_CanBeEmpty | Onyx::OverlayDockNodeFlag_StartWithTabBarHidden,
                                Onyx::OverlayWindowFlag_NoBackground | Onyx::OverlayWindowFlag_MousePassThrough |
                                    Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);

        editorWindow_Draw();
        hierarchyWindow_Draw();
        entityWindow_Draw();
        assetBrowserWindow_Draw();
        consoleWindow_Draw();
        renderingWindow_Draw();
        viewportWindow_Draw();

        editor_ControlCamera();

        Scene_Render(s_Data->ActiveScene);

        ov->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
}

void Terminate()
{
    // window and overlay are destroyed automatically
    s_Data.Destruct();
    Onyx::Terminate();
}
} // namespace Engine
