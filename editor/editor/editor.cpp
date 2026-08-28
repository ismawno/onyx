#include "editor.hpp"
#include "scene.hpp"
#include "components.hpp"
#include "onyx/resources.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"

namespace Editor
{
struct IdLabelData
{
    Onyx::LayoutId MainDockSpace = "__onyx_editor_Main_dockspace";
    Onyx::LayoutId EditorDockSpace = "__onyx_editor_Dockspace";

    Onyx::OverlayLabel Editor = "Editor";
    Onyx::OverlayLabel Hierarchy = "Hierarchy";
    Onyx::OverlayLabel MainViewport = "Viewport 0";
    Onyx::OverlayLabel Entity = "Entity";
    Onyx::OverlayLabel Console = "Console";
    Onyx::OverlayLabel AssetBrowser = "Asset browser";
    Onyx::OverlayLabel Rendering = "Rendering";
};

struct Viewport
{
    const Onyx::OverlayWindow *Window = nullptr;
    Onyx::RenderTexture *Target = nullptr;
    TKit::TierString Title{};

    f32v2 Position{0.f};
    f32v2 Size{0.f};
    u32v2 Resolution{0};
    f32 Aspect = 0.f;
    bool Visible = true;
};

struct EditorData
{
    Onyx::Window *Window;
    Onyx::Overlay *Overlay;

    TKit::TierArray<Viewport> Viewports{};

    const IdLabelData Labels{};
    Scene ActiveScene{};
    Entity SelectedEntity = TKit::NullEntity;
    TKit::StaticArray8<Onyx::Camera<D2>> Cameras2{};
    TKit::StaticArray8<Onyx::Camera<D3>> Cameras3{};

    template <Dimension D> TKit::StaticArray8<Onyx::Camera<D>> &GetCameras()
    {
        if constexpr (D == D2)
            return Cameras2;
        else
            return Cameras3;
    }
};

static TKit::Storage<EditorData> s_Data{};

static Onyx::Window *init_CreateWindow()
{
#ifdef TKIT_ENABLE_ENSURE
    const char *title = "Onyx editor - " ONYX_VERSION " - [DEBUG]";
#else
    const char *title = "Onyx editor - " ONYX_VERSION;
#endif
    return Onyx::OpenWindow({.Window = {.Title = title}});
}

template <Dimension D> void init_CreateDefaultCameraAndViewFromTarget(Onyx::RenderTexture *rt)
{
    TKit::StaticArray8<Onyx::Camera<D>> &cams = s_Data->GetCameras<D>();
    Onyx::Camera<D> &cam = cams.Append();
    Onyx::RenderView<D> *rv = rt->CreateRenderView<D>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    s_Data->ActiveScene.AddTarget(rv);
}

static Viewport init_CreateViewport(const u32v2 resolution)
{
    Viewport vp;
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture(resolution);

    vp.Target = rt;
    vp.Position = 0.f;
    vp.Size = 1.f;
    vp.Resolution = resolution;
    vp.Aspect = 16.f / 9.f;

    return vp;
}
static Onyx::Overlay *init_CreateOverlay(Onyx::Window *win)
{
    Onyx::Overlay *ov = win->CreateOverlay({.Flags = Onyx::OverlayFlag_Docking | Onyx::OverlayFlag_AutoSerialize});

    // TODO(Isma): Change this with a project-specific path!
    if (!ov->Deserialize("."))
    {
        const IdLabelData &idData = s_Data->Labels;
        ov->DeclareWindow(idData.Editor.Id);
        ov->DeclareDockSpace(idData.EditorDockSpace.Id, idData.Editor.Id);

        const Onyx::OverlayDockNode *mainTree = Onyx::DockTabBar(idData.Editor.Id);
        const Onyx::OverlayDockNode *editorTree = Onyx::DockSplit(
            Onyx::LayoutAxis_Vertical, 0.15f,
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f, Onyx::DockTabBar(idData.Hierarchy.Id),
                            Onyx::DockTabBar(idData.Entity.Id)),
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f,
                            Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.65f, Onyx::DockTabBar(idData.MainViewport.Id),
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

    s_Data->Window = init_CreateWindow();
    s_Data->Overlay = init_CreateOverlay(s_Data->Window);
    s_Data->Viewports.Append(init_CreateViewport({1920, 1080}));

    Onyx::RenderTexture *rt = s_Data->Viewports.GetFront().Target;
    init_CreateDefaultCameraAndViewFromTarget<D2>(rt);
    init_CreateDefaultCameraAndViewFromTarget<D3>(rt);
}

static void editorWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;

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
    TKit::TierArray<Viewport> &vps = s_Data->Viewports;
    if (vps.IsEmpty())
        return;

    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;

    const auto drawViewportWindow = [&](Viewport &vp, const Onyx::LayoutId id, const Onyx::LayoutId panelId,
                                        const Onyx::OverlayLabel &winLabel) {
        vp.Title = winLabel.Title;
        if (ov->BeginWindow(winLabel, &vp.Visible, Onyx::OverlayWindowFlag_MenuBar))
        {
            vp.Window = ov->GetActiveWindow();

            const Onyx::LayoutElementQueryInfo *vpParentElm = ov->QueryElement(panelId);
            const Onyx::LayoutElementQueryInfo *vpElm = ov->QueryElement(id);

            ov->BeginPanel(panelId, Onyx::LayoutPanelParameters{.Alignment = Onyx::Alignment_Center,
                                                                .Sizing = Onyx::LayoutSizing::Grow()});
            if (vpElm)
                vp.Position = vpElm->Position;

            if (vpParentElm)
            {
                const f32v2 &size = vpParentElm->Size;
                const f32 aspect = size[0] / size[1];

                f32 w;
                f32 h;
                if (aspect < vp.Aspect)
                {
                    w = vpParentElm->Size[0];
                    h = w / vp.Aspect;
                }
                else
                {
                    h = vpParentElm->Size[1];
                    w = h * vp.Aspect;
                }

                vp.Size = f32v2{w, h};
                ov->Image(id, *vp.Target, vp.Size);
            }
            else
                ov->Image(id, *vp.Target, Onyx::LayoutSizing::Grow());
            ov->EndPanel();

            if (ov->BeginMenuBar())
            {
                if (ov->BeginMenu("Resolution"))
                {
                    ov->BeginScroll("Scroll", 200.f,
                                    Onyx::OverlayScrollFlag_NoBackground | Onyx::OverlayScrollFlag_FlexWidth);
                    const auto addResolution = [&](const char *name, const u32 w, const u32 h) {
                        const u32v2 r = u32v2{w, h};
                        if (ov->MenuItem(name, vp.Resolution == r))
                        {
                            vp.Target->Resize(r);
                            vp.Resolution = r;
                            vp.Aspect = f32(w) / f32(h);
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
    const u32 size = vps.GetSize();
    const u32 idOffset = 10 * size;
    drawViewportWindow(vps[0], 0u, idOffset, idData.MainViewport);
    for (u32 i = 1; i < size; ++i)
    {
        const TKit::StackString title = TKit::StackString::Format("Viewport {}", i);
        drawViewportWindow(vps[i], i, i + idOffset, {i + 2 * idOffset, title});
    }
}
static void hierarchyWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Hierarchy))
    {
        Scene &scene = s_Data->ActiveScene;
        if (ov->Button("Add entity", Onyx::OverlayButtonFlag_SpanFullWidth))
            scene.CreateEntity("Entity");

        TKit::Registry &r = scene.Registry;
        r.Query<NameComponent>().Each([&](const Entity e, const NameComponent &name) {
            if (ov->Selectable({e, name.Name}, e == s_Data->SelectedEntity))
                s_Data->SelectedEntity = e;
        });

        ov->EndWindow();
    }
}
template <typename C, typename... Args>
static bool entityWindow_ChooseComponent(const Entity e, const char *name, TKit::Registry &registry, Onyx::Overlay *ov,
                                         Args &&...args)
{
    if (registry.HasComponent<C>(e))
        return true;
    if (ov->Button(name, Onyx::OverlayButtonFlag_SpanFullWidth))
        registry.AddComponent<C>(e, std::forward<Args>(args)...);
    return false;
}
template <template <Dimension> typename C>
static bool entityWindow_ChooseComponent(const Entity e, const char *name, TKit::Registry &registry, Onyx::Overlay *ov,
                                         const C<D2> &c2 = {}, const C<D3> &c3 = {})
{
    if (registry.HasComponent<C<D2>>(e) || registry.HasComponent<C<D3>>(e))
        return true;

    ov->BeginPanel(Onyx::LayoutPanelParameters{.Direction = Onyx::LayoutDirection_LeftToRight,
                                               .Alignment = ov->TopLeft,
                                               .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                               .ChildGap = ov->GetStyle()[Onyx::OverlayStyle_ChildGap]});

    ov->TextRaw(name);

    ov->Panel(Onyx::LayoutPanelParameters{.Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Grow()}});

    ov->PushId(name);
    if (ov->Button("2D"))
        registry.AddComponent<C<D2>>(e, c2);
    if (ov->Button("3D"))
        registry.AddComponent<C<D3>>(e, c3);
    ov->PopId();

    ov->EndPanel();
    return false;
}
static void entityWindow_DisplayComponents(const Entity e, const TKit::Registry &registry, Onyx::Overlay *ov)
{
    NameComponent *name = registry.GetComponent<NameComponent>(e);
    TKIT_ASSERT(name, "[ONYX][EDITOR] All entities in the editor must have a name");

    constexpr u32 size = 64;
    char buf[size];

    const u32 nameSize = Math::Min(name->Name.GetSize(), size - 1);
    for (u32 i = 0; i < nameSize; ++i)
        buf[i] = name->Name[i];
    buf[nameSize] = 0;

    if (ov->InputText("Name", buf, size))
        name->Name = buf;
}
template <Dimension D>
static void entityWindow_DisplayComponents(const Entity e, const TKit::Registry &registry, Onyx::Overlay *ov)
{
    TransformComponent<D> *transform = registry.GetComponent<TransformComponent<D>>(e);
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
            ov->HorizontalDrag("Rotation", &t.Rotation, speed);
        else
        {
            f32q &q = t.Rotation;
            f32v3 euler = Math::ToEulerAngles(q);
            if (ov->HorizontalDrag("Rotation", &euler, speed))
                q = f32q::FromEulerAngles(euler);
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
}

static void entityWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;
    const Entity e = s_Data->SelectedEntity;
    if (e != NullEntity && ov->BeginWindow(idData.Entity))
    {
        Scene &scene = s_Data->ActiveScene;
        TKit::Registry &r = scene.Registry;

        if (ov->BeginPopup("Components"))
        {
            const Onyx::DefaultResources &defRes = Onyx::Resources::GetDefaultResources();

            bool hasAllComponents = entityWindow_ChooseComponent<TransformComponent>(e, "Transform", r, ov);
            hasAllComponents &= entityWindow_ChooseComponent<StaticMeshComponent>(e, "Static mesh", r, ov,
                                                                                  {1, defRes.Quad2}, {1, defRes.Quad3});
            hasAllComponents &= entityWindow_ChooseComponent<RenderContextComponent>(
                e, "Render context", r, ov, {scene.GetMainRenderContext<D2>()}, {scene.GetMainRenderContext<D3>()});

            if (hasAllComponents)
                ov->CloseCurrentPopup();
            ov->EndPopup();
        }

        if (ov->Button("Add component", Onyx::OverlayButtonFlag_SpanFullWidth))
            ov->OpenPopup("Components");

        entityWindow_DisplayComponents(e, r, ov);
        entityWindow_DisplayComponents<D2>(e, r, ov);
        entityWindow_DisplayComponents<D3>(e, r, ov);

        ov->EndWindow();
    }
}
static void assetBrowserWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.AssetBrowser))
    {
        ov->EndWindow();
    }
}
static void consoleWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Console))
    {
        ov->EndWindow();
    }
}
template <Dimension D> static void renderingWindow_DisplayViews(const char *name, const Viewport &vp, Onyx::Overlay *ov)
{
    const Onyx::RenderTexture *rt = vp.Target;
    const TKit::StaticArray<Onyx::RenderView<D> *, ONYX_MAX_VIEWS> &views = rt->GetRenderViews<D>();
    if (!views.IsEmpty())
    {
        ov->HorizontalSeparator(name);
        u32 index = 0;
        for (Onyx::RenderView<D> *rv : views)
        {
            const TKit::StackString title = TKit::StackString::Format("View {}", index++);
            if (ov->PushTree({rv, title}, Onyx::OverlayTreeFlag_DrawLines))
            {
                ov->ColorEditor("Background color", &rv->ClearColor);

                ov->HorizontalSeparator("Viewport");
                Onyx::RenderViewFlags flags = rv->GetFlags();

                const bool nv = flags & Onyx::RenderViewFlag_NormalizedViewportCoordinates;
                const auto getVp =
                    nv ? &Onyx::RenderView<D>::GetNormalizedViewport : &Onyx::RenderView<D>::GetAbsoluteViewport;
                const auto setVp =
                    nv ? &Onyx::RenderView<D>::SetNormalizedViewport : &Onyx::RenderView<D>::SetAbsoluteViewport;

                const bool ns = flags & Onyx::RenderViewFlag_NormalizedScissorCoordinates;
                const auto getSc =
                    ns ? &Onyx::RenderView<D>::GetNormalizedScissor : &Onyx::RenderView<D>::GetAbsoluteScissor;
                const auto setSc =
                    ns ? &Onyx::RenderView<D>::SetNormalizedScissor : &Onyx::RenderView<D>::SetAbsoluteScissor;

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

                changed =
                    ov->CheckBoxFlags("Normalized coordinates", &flags, Onyx::RenderViewFlag_NormalizedCoordinates);
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
                    ov->SetItemTooltipRaw("Outlines can only be enabled with post-processing");

                changed |= ov->CheckBoxFlags("Transparency", &flags, Onyx::RenderViewFlag_Transparency);
                changed |= ov->CheckBoxFlags("Hidden", &flags, Onyx::RenderViewFlag_Hidden);

                if (changed)
                    rv->SetFlags(flags);

                ov->PopTree();
            }
        }
    }
}
static void renderingWindow_Draw()
{
    Onyx::Overlay *ov = s_Data->Overlay;
    const IdLabelData &idData = s_Data->Labels;
    if (ov->BeginWindow(idData.Rendering))
    {
        if (ov->PushTree("Viewports", Onyx::OverlayTreeFlag_DrawLines))
        {
            for (Viewport &vp : s_Data->Viewports)
            {
                ov->HorizontalSeparator(vp.Title);
                ov->Text("Resolution: {}x{}", vp.Resolution[0], vp.Resolution[1]);
                ov->CheckBox("Visible", &vp.Visible);

                ov->PushDirection(Onyx::LayoutDirection_LeftToRight);
                ov->Button("Add 2D view", Onyx::OverlayButtonFlag_SpanFullWidth);
                ov->Button("Add 3D view", Onyx::OverlayButtonFlag_SpanFullWidth);
                ov->PopDirection();

                renderingWindow_DisplayViews<D2>("2D Views", vp, ov);
                renderingWindow_DisplayViews<D3>("3D Views", vp, ov);
            }
            ov->PopTree();
        }
        ov->EndWindow();
    }
}

template <Dimension D> static Onyx::RenderView<D> *editor_GetHoveredView(const Viewport &vp, f32v2 *outMpos = nullptr)
{
    Onyx::Window *win = s_Data->Window;
    const Onyx::RenderTexture *rt = vp.Target;

    const auto views = rt->GetSortedViews<D>();
    const f32v2 vpScreenPos =
        s_Data->Overlay->GetMainNativeWindow()->ToLocalScreen(vp.Position + f32v2{0.f, vp.Size[1]});

    const f32v2 ampos = win->GetAbsoluteMousePosition() - vpScreenPos;
    const f32v2 nmpos = ampos / vp.Size;
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
    for (const Viewport &vp : s_Data->Viewports)
        if (vp.Window->IsHovered())
        {
            Onyx::RenderView<D> *rv = editor_GetHoveredView<D>(vp);
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
    for (const Viewport &vp : s_Data->Viewports)
        if (vp.Window->IsHovered())
        {
            f32v2 mpos;
            Onyx::RenderView<D> *rv = editor_GetHoveredView<D>(vp, &mpos);
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
    const IdLabelData &idData = s_Data->Labels;

    ov->FullScreenDockSpace(idData.MainDockSpace,
                            Onyx::OverlayDockNodeFlag_CanBeEmpty | Onyx::OverlayDockNodeFlag_StartWithTabBarHidden,
                            Onyx::OverlayWindowFlag_NoBackground | Onyx::OverlayWindowFlag_MousePassThrough |
                                Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);

    editorWindow_Draw();
    viewportWindow_Draw();
    hierarchyWindow_Draw();
    entityWindow_Draw();
    assetBrowserWindow_Draw();
    consoleWindow_Draw();
    renderingWindow_Draw();

    editor_ControlCamera();

    s_Data->ActiveScene.Draw();

    ov->Draw();

    Onyx::Transfer();
    Onyx::Render();
}

void Terminate()
{
    // window and overlay are destroyed automatically
    s_Data.Destruct();
    Onyx::Terminate();
}
} // namespace Editor
