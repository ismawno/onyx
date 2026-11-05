#include "components.hpp"
#include "editor_project.hpp"
#include "editor_undo_redo.hpp"
#include "editor_console.hpp"
#include "onyx/resources.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"
#include "onyx/dialog.hpp"

#define ONYX_NAME_BUF_SIZE 64

#define NAME_BY_DIM(dim, name) (scene.HasBothDimensions() ? (dim " " name) : (name))

namespace Engine
{
struct Labels
{
    Onyx::LayoutId MainDockSpace = "__onyx_editor_Main_dockspace";
    Onyx::LayoutId EditorDockSpace = "__onyx_editor_Dockspace";
    Onyx::LayoutId EditorWindow = "__onyx_editor_Window";

    Onyx::OverlayLabel Hierarchy = "Hierarchy";
    Onyx::OverlayLabel Entity = "Entity";
    Onyx::OverlayLabel Console = "Console";
    Onyx::OverlayLabel AssetBrowser = "Asset browser";
    Onyx::OverlayLabel Scene = "Scene";
};

struct Editor
{
    Onyx::Window *Window = nullptr;
    Onyx::Overlay *Overlay = nullptr;

    Console Console{};
    UndoRedo UndoRedo{};

    EditorProject Project{};
};

static TKit::Storage<Labels> s_Labels{};
static TKit::Storage<Editor> s_Editor{};

static Onyx::Window *createMainWindow()
{
#ifdef TKIT_ENABLE_ENSURE
    const char *title = "Onyx editor - " ONYX_VERSION " - [DEBUG]";
#else
    const char *title = "Onyx editor - " ONYX_VERSION;
#endif
    return Onyx::OpenWindow({.Window = {.Title = title}});
}

static Onyx::Overlay *createMainOverlay(Onyx::Window *win, const fs::path &deserializePath)
{
    Onyx::Overlay *ov = win->CreateOverlay({.Flags = Onyx::OverlayFlag_Docking | Onyx::OverlayFlag_AutoSerialize});
    if (!ov->Deserialize(deserializePath))
    {
        const Onyx::LayoutId mainViewportId = 0u;
        const Labels &ids = *s_Labels;

        ov->DeclareWindow(ids.EditorWindow);
        ov->DeclareDockSpace(ids.EditorDockSpace.Id, ids.EditorWindow);

        const Onyx::OverlayDockNode *mainTree = Onyx::DockTabBar(ids.EditorWindow);
        const Onyx::OverlayDockNode *editorTree = Onyx::DockSplit(
            Onyx::LayoutAxis_Vertical, 0.15f,
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f, Onyx::DockTabBar(ids.Hierarchy.Id),
                            Onyx::DockTabBar(ids.Entity.Id)),
            Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.65f,
                            Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.65f, Onyx::DockTabBar(mainViewportId),
                                            Onyx::DockTabBar(ids.Scene.Id)),
                            Onyx::DockTabBar({ids.Console.Id, ids.AssetBrowser.Id})));

        ov->ApplyDockTree(ids.MainDockSpace, mainTree);
        ov->ApplyDockTree(ids.EditorDockSpace, editorTree);
    }

    return ov;
}

struct ProjectLauncherResult
{
    EditorProject Project{};
    bool Canceled = true;
};

struct ProjectLauncherDrawInfo
{
    ProjectLauncherDrawInfo()
    {
        const auto res = LoadRecentProjects();
        TKIT_LOG_ERROR_IF(!res, "[ONYX][EDITOR] Failed to load recent projects: {}", res.GetError().ToString());
        if (res)
            RecentProjects = *res;
    }
    const char *DialogError = nullptr;
    TKit::TierString SerializeError{};
    TKit::TierString DeserializeError{};

    fs::path NewProjectDir = GetHomePath();
    fs::path ConfigPath = GetConfigPath() / "config.yaml";
    TKit::TierArray<fs::path> RecentProjects;

    fs::path ProjectToRemove{};
    u32 RecentIndex = TKIT_U32_MAX;
    u32 StartDimension = D2;
    char NewProjectName[64] = "onyx-project";
    bool RequestOpenRecent = false;
    bool Creating = false;
    bool DefaultScene = true;
};

static bool drawProjectLauncher(Onyx::Overlay *ov, ProjectLauncherDrawInfo &info, ProjectLauncherResult &res)
{
    bool goOn = true;
    const auto openDialog = [&](fs::path &dir) {
        info.DialogError = nullptr;
        Onyx::ClearDialogError();

        const auto res = Onyx::OpenFolderDialog();
        if (res)
            dir = *res;
        else
        {
            const Onyx::DialogStatus status = res.GetError();
            TKIT_ASSERT(status != Onyx::Dialog_Success, "[ONYX][EDITOR] Cannot have a success status on an error path");
            info.DialogError = status == Onyx::Dialog_Cancel ? "Operation canceled" : Onyx::GetDialogError();
        }
    };
    const auto displayError = [&](const char *err) {
        if (err)
        {
            ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Salmon);
            ov->TextRaw(err);
            ov->PopStyleColor();
        }
    };

    const auto displaySerializeError = [&] {
        displayError(info.SerializeError.IsEmpty() ? nullptr : info.SerializeError.CString());
    };
    const auto displayDeserializeError = [&] {
        displayError(info.DeserializeError.IsEmpty() ? nullptr : info.DeserializeError.CString());
    };

#ifdef TKIT_ENABLE_ENSURE
    ov->PushPanel(Onyx::Alignment_Center, Onyx::LayoutSizing::Fit());
    ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
    ov->TextRaw("Warning - Running on a debug build. Expect reduced performance");
    ov->PopStyleColor();
    ov->PopPanel();
#endif

    displaySerializeError();
    displayDeserializeError();
    if (info.Creating)
    {
        ov->InputText("Project name", info.NewProjectName, 64);
        ov->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Alignment_Center, Onyx::LayoutSizing::Fit());
        if (ov->Button("Browse"))
            openDialog(info.NewProjectDir);

        const fs::path ppath = info.NewProjectDir / info.NewProjectName;
        ov->Text("Project path: {}", ppath.string());
        ov->PopPanel();

        displayError(info.DialogError);

        ov->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::LayoutSizing::Fit());
        ov->CheckBox("Default scene", &info.DefaultScene);
        ov->PopPanel();
        ov->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::LayoutSizing::Fit());
        ov->RadioButton("2D", &info.StartDimension, D2);
        ov->RadioButton("3D", &info.StartDimension, D3);
        ov->RadioButton("Both", &info.StartDimension, 0);
        ov->PopPanel();

        ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
        info.Creating = !ov->Button("Go back", Onyx::OverlayButtonFlag_SpanFullWidth);

        const bool exists = fs::exists(ppath);
        const auto create = [&] {
            // this should not fail, and be the end of this loop. thats why we dont bother cleaning up Project:
            // should be already cleaned
            res.Project.Name = info.NewProjectName;
            res.Project.Path = ppath;
            if (info.DefaultScene)
            {
                if (info.StartDimension == D2)
                    res.Project.ActiveScene = res.Project.CreateDefaultScene<D2>();
                else
                    res.Project.ActiveScene = res.Project.CreateDefaultScene<D3>();
            }
            res.Project.Serialize(ppath);
            res.Canceled = false;
            UpdateRecentProjects(ppath);

            goOn = false;
        };

        if (ov->BeginPopup("Warning", Onyx::OverlayWindowFlag_AutoResize | Onyx::OverlayWindowFlag_PopupModal |
                                          Onyx::OverlayWindowFlag_PopupDoNotPlaceAtMouse |
                                          Onyx::OverlayWindowFlag_NoHeaderBar))
        {
            ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
            ov->TextRaw("Warning - The specified path already exists");
            ov->PopStyleColor();
            ov->Text("Target: {}", ppath.string());

            ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
            if (ov->Button("Cancel", Onyx::OverlayButtonFlag_SpanFullWidth))
                ov->CloseCurrentPopup();

            ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Salmon);

            if (ov->Button("Overwrite", Onyx::OverlayButtonFlag_SpanFullWidth))
            {
                create();
                fs::remove_all(ppath);
            }

            ov->PopStyleColor();
            ov->PopPanel();
            ov->EndPopup();
        }

        if (ov->Button("Create", Onyx::OverlayButtonFlag_SpanFullWidth))
        {
            if (exists)
                ov->OpenPopup("Warning");
            else
                create();
        }

        ov->PopPanel();
    }
    else
    {
        ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
        info.Creating = ov->Button("Create project", Onyx::OverlayButtonFlag_SpanFullWidth);

        const bool recentSelected =
            info.RecentIndex != TKIT_U32_MAX && fs::exists(info.RecentProjects[info.RecentIndex]);
        if (ov->Button({ov, recentSelected ? "Open project" : "Browse projects"},
                       Onyx::OverlayButtonFlag_SpanFullWidth) ||
            info.RequestOpenRecent)
        {
            fs::path ppath;
            if (recentSelected)
                ppath = info.RecentProjects[info.RecentIndex];
            else
            {
                openDialog(ppath);
                TKIT_ASSERT(!info.RequestOpenRecent,
                            "[ONYX][EDITOR] Can only request open a recent project if a recent project is selected");
            }

            info.RequestOpenRecent = false;

            if (!info.DialogError)
            {
                const auto desRes = EditorProject::Deserialize(ppath);
                if (desRes)
                {
                    res.Project = *desRes;
                    res.Canceled = false;
                    UpdateRecentProjects(ppath);
                    goOn = false;
                }
                else
                    info.DeserializeError = desRes.GetError().ToString();
            }
        }
        ov->PopPanel();

        if (!info.RecentProjects.IsEmpty())
        {
            if (ov->BeginPopup("Erase project", Onyx::OverlayWindowFlag_AutoResize |
                                                    Onyx::OverlayWindowFlag_PopupModal |
                                                    Onyx::OverlayWindowFlag_PopupDoNotPlaceAtMouse |
                                                    Onyx::OverlayWindowFlag_NoHeaderBar))
            {
                ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
                ov->TextRaw("Warning - The project folder and all underlying content will be removed. This action is "
                            "irreversible");
                ov->PopStyleColor();
                ov->Text("Target: {}", info.ProjectToRemove.string());

                ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
                if (ov->Button("Cancel", Onyx::OverlayButtonFlag_SpanFullWidth))
                    ov->CloseCurrentPopup();

                ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Salmon);
                if (ov->Button("Proceed", Onyx::OverlayButtonFlag_SpanFullWidth))
                {
                    fs::remove_all(info.ProjectToRemove);
                    ov->CloseCurrentPopup();
                }
                ov->PopStyleColor();
                ov->PopPanel();

                ov->EndPopup();
            }
            ov->BeginScroll("Recent projects", ov->GetStyle(Onyx::OverlayStyle_ListBoxMaxHeight),
                            Onyx::OverlayScrollFlag_Tight | Onyx::OverlayScrollFlag_Borders |
                                Onyx::OverlayScrollFlag_Title | Onyx::OverlayScrollFlag_FlexWidth);

            for (u32 i = 0; i < info.RecentProjects.GetSize(); ++i)
            {
                const bool selected = i == info.RecentIndex;
                const fs::path &p = info.RecentProjects[i];
                ov->PushId(&p);

                const bool missing = !fs::exists(p);

                ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
                ov->BeginDisabled(missing);
                if (ov->Button(Onyx::CodePoint_Cross))
                {
                    info.ProjectToRemove = p;
                    ov->OpenPopup("Erase project");
                }
                ov->EndDisabled();

                const bool clicked = ov->BeginSelectable("Project", selected);
                const bool dclicked = ov->IsItemDoubleClicked();
                if (dclicked)
                {
                    info.RecentIndex = i;
                    info.RequestOpenRecent = true;
                }
                else if (clicked)
                    info.RecentIndex = selected ? TKIT_U32_MAX : i;

                ov->PushPanel(Onyx::LayoutDirection_LeftToRight);

                if (missing)
                    ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Salmon);

                ov->TextRaw(p.filename().string());
                if (missing)
                {
                    ov->SetItemTooltipRaw("This project has been deleted!", Onyx::OverlayFocusFlag_NormalDelay);
                    ov->PopStyleColor();
                }

                ov->BeginDisabled();
                ov->TextRaw(p.string());
                ov->EndDisabled();

                ov->PopPanel();

                ov->EndSelectable();
                ov->PopPanel();

                ov->PopId();
            }
            ov->EndScroll();
        }

        displayError(info.DialogError);
    }

    return goOn;
}

static ProjectLauncherResult runProjectLauncher()
{
    Onyx::Overlay *ov = Onyx::CreateFloatingOverlay();
    bool running = true;

    ProjectLauncherResult res;
    ProjectLauncherDrawInfo info{};
    while (Onyx::Running())
    {
#ifdef TKIT_ENABLE_ENSURE
        const char *title = "Onyx launcher - " ONYX_VERSION " - [DEBUG]";
#else
        const char *title = "Onyx launcher - " ONYX_VERSION;
#endif
        if (ov->BeginWindow(title, &running, Onyx::OverlayWindowFlag_AutoResize))
        {
            if (!drawProjectLauncher(ov, info, res))
                Onyx::Quit(Onyx::QuitFlag_DestroyFloatingOverlays);

            ov->EndWindow();
        }
        if (!running)
            Onyx::Quit(Onyx::QuitFlag_DestroyFloatingOverlays);

        ov->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }

    return res;
}

// TODO(Isma): Editor as a full class. Remove Data suffix from Editor, ConsoleData etc
// TODO(Isma): For sliders and drags, input numeric does not work
// TODO(Isma): Resolution change does nothing
// TODO(Isma): Overlay needs to abort if deserialization failed
// TODO(Isma): userLaunchDecision, the part where we create a default scene, that must move do the draw stuff, so that
// we dont serialize twice
// TODO(Isma): Improve error messages, tell the user what to do etc. Also, waited blabla for blabla, should be a
// warning, and tell the user what to do
// TODO(Isma): Fix naming
// TODO(Isma): Forward serializeEditor/deserializeEditor errors
// TODO(Isma): Expose errors when serializing/deserializing. create a macro to use TryRead, so that editor
// deserialization may fail
// TODO(Isma): Add a way to edit project settings (window promotions for instance)
// TODO(Isma): Add a way to open/create new projects from editor (use same popup)
// TODO(Isma): Disable promotion when parent window is fullscreen
// TODO(Isma): Console is extremely bugged?
// TODO(Isma): Log the project save action into the console
// TODO(Isma): Implement undo/redo on all editables
// TODO(Isma): Implement a custom instance data API and a way to hook shaders into the existing pipeline
// TODO(Isma): Implement a custom "mini"-graphics API to submit your own pipelines
// TODO(Isma): Implement a way of selecting entities from the viewport
// TODO(Isma): Implement guizmos

static void setProject(const EditorProject &p)
{
    s_Editor->Project.Destroy();
    s_Editor->UndoRedo.Clear();
    s_Editor->Console.Clear();
    s_Editor->Project = p;
}

bool Initialize()
{
    Onyx::Initialize();
    Onyx::Resources_CreateDefault();

    s_Labels.Construct();
    s_Editor.Construct();

    const ProjectLauncherResult res = runProjectLauncher();
    if (res.Canceled)
    {
        Terminate();
        return false;
    }
    s_Editor->Window = createMainWindow();
    s_Editor->Overlay = createMainOverlay(s_Editor->Window, s_Editor->Project.GetOverlayLayoutPath());
    setProject(res.Project);

    SetConsoleLogCallback(s_Editor->Console);
    return true;
}

static void drawEditorWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    static ProjectLauncherDrawInfo launcherInfo{};

    ov->PushStyleVar(Onyx::OverlayStyle_ContentAreaPadding, 0.f);
    const bool opened = ov->BeginWindow({ids.EditorWindow, s_Editor->Project.Name}, Onyx::OverlayWindowFlag_MenuBar);
    ov->PopStyleVar();
    if (opened)
    {
        ov->SetNextWindowPosition(ov->GetActiveNativeWindow()->GetLocalScreenCenter(), Onyx::OverlayCondition_Appearing,
                                  -0.5f);
        if (ov->BeginPopup("Launcher", Onyx::OverlayWindowFlag_AutoResize | Onyx::OverlayWindowFlag_PopupModal |
                                           Onyx::OverlayWindowFlag_PopupDoNotPlaceAtMouse))
        {
            ProjectLauncherResult res;
            if (!drawProjectLauncher(ov, launcherInfo, res))
            {
                if (!res.Canceled)
                    setProject(res.Project);
                ov->CloseCurrentPopup();
            }

            ov->EndPopup();
        }
        ov->DockSpace(ids.EditorDockSpace, Onyx::OverlayDockNodeFlag_CanBeEmpty, Onyx::OverlayWindowFlag_ChildGrow);
        if (ov->BeginMenuBar())
        {
            if (ov->BeginMenu("File"))
            {
                if (ov->MenuItem("Open launcher"))
                {
                    ov->OpenPopup("Launcher");
                    launcherInfo = {};
                }
                if (ov->MenuItem("Save project"))
                    s_Editor->Project.Serialize();
                if (ov->MenuItem("Save as..."))
                {
                    const auto res = Onyx::OpenFolderDialog();
                    if (res)
                    {
                        const fs::path &ppath = *res;
                        s_Editor->Project.Serialize(ppath);
                    }
                }
                ov->EndMenu();
            }
            ov->EndMenuBar();
        }
        ov->EndWindow();
    }
}
static void drawViewportWindow()
{
    TKit::TierHive<EditorViewport> &vps = s_Editor->Project.GetActiveScene().Viewports;
    if (vps.IsEmpty())
        return;

    Onyx::Overlay *ov = s_Editor->Overlay;
    const EditorScene &scene = s_Editor->Project.GetActiveScene();
    const auto drawViewportWindow = [&](const Viewport vp) {
        EditorViewport &viewport = vps[vp];
        if (ov->BeginWindow({vp, viewport.Name}, &viewport.Visible, Onyx::OverlayWindowFlag_MenuBar))
        {
            const char *warning = nullptr;
            const bool e2 = viewport.GetViews<D2>().IsEmpty();
            const bool e3 = viewport.GetViews<D3>().IsEmpty();
            if (scene.HasBothDimensions() && e2 && e3)
                warning = "This viewport has no views and will not display contents until one is created from the "
                          "'Scene' tab";
            else if (scene.IsDimension<D2>() && e2)
                warning = "This viewport has no 2D views and will not display contents until one is created from the "
                          "'Scene' tab";
            else if (scene.IsDimension<D3>() && e3)
                warning = "This viewport has no 3D views and will not display contents until one is created from the "
                          "'Scene' tab";

            if (warning)
            {
                ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
                ov->TextRaw(Onyx::TextMode_Wrapped, warning);
                ov->PopStyleColor();
            }

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
static void drawHierarchyWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    if (ov->BeginWindow(ids.Hierarchy))
    {
        EditorScene &scene = s_Editor->Project.GetActiveScene();
        TKit::Registry &r = scene.Registry;
        if (ov->Button("Add entity", Onyx::OverlayButtonFlag_SpanFullWidth))
        {
            const Entity e = r.CreateEntity();
            r.AddComponent<NameComponent>(e, ONYX_DEFAULT_NAME("Entity", e));
        }

        r.IterateEntitiesByInsertionOrder([&](const Entity e) {
            const NameComponent *nc = scene.Registry.GetComponent<NameComponent>(e);
            TKIT_ASSERT(nc, "[ONYX][EDITOR] All editor entities must have a name, but entity {} does not", e);
            ov->PushPanel(Onyx::LayoutDirection_LeftToRight);

            ov->PushId(e);
            const bool selected = e == scene.SelectedEntity;
            if (ov->Button(Onyx::CodePoint_Cross))
            {
                scene.Registry.DestroyEntity(e);
                if (selected)
                    scene.SelectedEntity = NullEntity;
            }
            if (ov->Selectable(nc->Name, selected))
                scene.SelectedEntity = e;
            ov->PopId();

            ov->PopPanel();
        });

        ov->EndWindow();
    }
}
template <typename C, typename... Args>
static void chooseComponent(const Entity e, const char *name, TKit::Registry &registry, Args &&...args)
{
    if (registry.HasComponent<C>(e))
        return;

    Onyx::Overlay *ov = s_Editor->Overlay;
    if (ov->Button(name, Onyx::OverlayButtonFlag_SpanFullWidth))
        registry.AddComponent<C>(e, std::forward<Args>(args)...);
}
template <template <Dimension> typename C>
static void chooseComponent(const Entity e, const char *name, TKit::Registry &registry, const C<D2> &c2 = {},
                            const C<D3> &c3 = {})
{
    const bool has2 = registry.HasComponent<C<D2>>(e);
    const bool has3 = registry.HasComponent<C<D3>>(e);
    if (has2 && has3)
        return;

    Onyx::Overlay *ov = s_Editor->Overlay;
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
static void displayComponents(const Entity e, const TKit::Registry &registry)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    NameComponent *name = registry.GetComponent<NameComponent>(e);
    if (name)
        ov->InputText("Name", &name->Name, ONYX_NAME_BUF_SIZE);
}
template <Dimension D> static void displayComponents(const Entity e, const TKit::Registry &registry)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
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

        const Onyx::DefaultResources &defRes = Onyx::Resources_GetDefault();
        TKit::StackArray<Onyx::Resource> defShapes{};

        defShapes.Reserve(5);
        defShapes.Append(defRes.GetResources<D>().Triangle);
        defShapes.Append(defRes.GetResources<D>().Quad);
        if constexpr (D == D2)
            ov->DropDown("Shape", &statMesh->Type, "Triangle#Quad");
        else
        {
            defShapes.Append(defRes.Resources3.Box);
            defShapes.Append(defRes.Resources3.Sphere);
            defShapes.Append(defRes.Resources3.Cylinder);
            ov->DropDown("Shape", &statMesh->Type, "Triangle#Quad#Box#Sphere#Cylinder");
        }
        statMesh->Mesh = defShapes[statMesh->Type];
        ov->ColorEditor("Base color", &statMesh->Color);
    }

    RenderContextComponent<D> *rc = registry.GetComponent<RenderContextComponent<D>>(e);
    if (rc)
    {
        if constexpr (D == D2)
            ov->HorizontalSeparator("Render context 2D");
        else
            ov->HorizontalSeparator("Render context 3D");

        const EditorScene &scene = s_Editor->Project.GetActiveScene();
        const TKit::TierHive<EditorContext<D>> &ctxs = scene.GetContexts<D>();

        if (!ctxs.IsEmpty())
        {
            const NameArray labels = CreateNameArray(ctxs);
            u32 selected = scene.FindContextInsertionIndex(rc->Context);
            ov->DropDown<TKit::StackString>("Context", &selected, labels.Names);

            const RenderContext handle = labels.GetId(selected);
            if (handle != NullRenderContext)
                rc->Context = ctxs[handle].Handle;
        }
        else
        {
            ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
            ov->TextRaw("No context has been added. Add one from the 'Scene' tab");
            ov->PopStyleColor();
        }
    }
    ov->PopId();
}

static void drawEntityWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    EditorScene &scene = s_Editor->Project.GetActiveScene();

    const Entity e = scene.SelectedEntity;
    if (e != NullEntity && ov->BeginWindow(ids.Entity))
    {
        TKit::Registry &r = scene.Registry;

        const auto hasAllComponents = [&] {
            if (scene.HasBothDimensions())
                return r.HasAllComponents(AllComponents{}, e);
            else if (scene.HasDimension<D2>())
                return r.HasAllComponents(AllComponentsWithDimension<D2>{}, e);
            return r.HasAllComponents(AllComponentsWithDimension<D3>{}, e);
        };

        if (ov->BeginPopup("Components"))
        {
            const Onyx::DefaultResources &defRes = Onyx::Resources_GetDefault();

            const StaticMeshComponent<D2> defSt2 = {defRes.Resources2.Quad, Onyx::Color_White, StaticMesh_Quad};
            const StaticMeshComponent<D3> defSt3 = {defRes.Resources3.Quad, Onyx::Color_White, StaticMesh_Quad};

            RenderContextComponent<D2> defCtx2 = {scene.Contexts2.IsEmpty() ? nullptr
                                                                            : scene.Contexts2.AtByIndex(0).Handle};
            RenderContextComponent<D3> defCtx3 = {scene.Contexts3.IsEmpty() ? nullptr
                                                                            : scene.Contexts3.AtByIndex(0).Handle};
            if (scene.HasBothDimensions())
            {
                chooseComponent<TransformComponent>(e, "Transform", r);
                chooseComponent<StaticMeshComponent>(e, "Static mesh", r, defSt2, defSt3);
                chooseComponent<RenderContextComponent>(e, "Render context", r, defCtx2, defCtx3);
            }
            else if (scene.HasDimension<D2>())
            {
                chooseComponent<TransformComponent<D2>>(e, "Transform", r);
                chooseComponent<StaticMeshComponent<D2>>(e, "Static mesh", r, defSt2);
                chooseComponent<RenderContextComponent<D2>>(e, "Render context", r, defCtx2);
            }
            else
            {
                chooseComponent<TransformComponent<D3>>(e, "Transform", r);
                chooseComponent<StaticMeshComponent<D3>>(e, "Static mesh", r, defSt3);
                chooseComponent<RenderContextComponent<D3>>(e, "Render context", r, defCtx3);
            }

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

        displayComponents(e, r);
        displayComponents<D2>(e, r);
        displayComponents<D3>(e, r);

        ov->EndWindow();
    }
}
static void drawAssetBrowserWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    if (ov->BeginWindow(ids.AssetBrowser))
    {
        ov->EndWindow();
    }
}
static void drawConsoleWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    if (ov->BeginWindow(ids.Console))
    {
        if (ov->Button("Clear"))
            s_Editor->Console.Clear();

        ov->HorizontalLine();
        s_Editor->Console.Each([&](const LogEntry &entry) {
            // fmt::println("[{:%Y-%m-%d %H:%M:%S}] [{}{}{}] {}", tm, color, level, TKIT_LOG_COLOR_RESET, message);
            ov->PushStyleVar(Onyx::OverlayStyle_ChildGap, 0.f);
            ov->PushPanel(Onyx::LayoutDirection_LeftToRight);

            ov->TextRaw("[");
            ov->PushStyleColor(Onyx::OverlayColor_Text, entry.Color);
            ov->TextRaw(entry.Level);
            ov->PopStyleColor();
            ov->Text("] {}", entry.Message);

            ov->PopPanel();
            ov->PopStyleVar();
            ov->HorizontalLine();
        });
        ov->EndWindow();
    }
}

struct ListBox
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

static bool drawListBox(const ListBox &params)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    ov->PushPanel(Onyx::LayoutDirection_LeftToRight);

    u32 *selected = params.Selected;
    const TKit::StackArray<TKit::StackString> &labels = *params.Labels;

    ov->PushPanel(Onyx::LayoutDirection_TopToBottom, Onyx::LayoutSizing::Fit());
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

    ov->PopPanel();

    const bool result =
        ov->ListBox<TKit::StackString>(params.Title, selected, labels, Onyx::OverlaySelectableFlag_ListBoxUnselect);
    ov->PopPanel();
    return result;
}

template <typename F1, typename F2> static void undoRedo_LambdaContinuousWidget(F1 &&undo, F2 &&redo)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    static LambdaCommand cmd{};
    if (ov->IsItemActivated())
        cmd.UndoFn = std::forward<F1>(undo);
    if (ov->IsItemDeactivated())
    {
        cmd.RedoFn = std::forward<F2>(redo);
        s_Editor->UndoRedo.Do(std::move(cmd));
    }
}
template <typename F> static void undoRedo_LambdaContinuousWidget(F &&fun)
{
    undoRedo_LambdaContinuousWidget(fun, fun);
}

template <Dimension D> static void displayView(EditorView<D> &view)
{
    Onyx::Overlay *ov = s_Editor->Overlay;

    ov->HorizontalSeparator(view.Name);
    ov->InputText("Name", &view.Name, ONYX_NAME_BUF_SIZE);

    // Note(Isma): this is weak bc of id recycling. consider implementing a generation for ids. this only prevents
    // crashing/ub
    const Scene sc = s_Editor->Project.ActiveScene;
    const Viewport vp = view.ParentViewport;
    const RenderView rvId = view.Id;

    const auto undoRedo_CheckViewExists = [=]() -> EditorView<D> * {
        if (!s_Editor->Project.Scenes.Contains(sc))
            return nullptr;
        EditorScene &scene = s_Editor->Project.Scenes[sc];

        if (!scene.Viewports.Contains(vp))
            return nullptr;

        EditorViewport &viewport = scene.Viewports[vp];
        if (!viewport.GetViews<D>().Contains(rvId))
            return nullptr;

        EditorView<D> &rView = viewport.GetViews<D>()[rvId];
        return &rView;
    };

    const auto undoRedo_CheckCameraExists = [=](const u32 idx) -> EditorCamera<D> * {
        if (!s_Editor->Project.Scenes.Contains(sc))
            return nullptr;

        EditorScene &scene = s_Editor->Project.Scenes[sc];
        if (idx < scene.GetCameras<D>().GetSize())
            return &scene.GetCameras<D>()[idx];
        return nullptr;
    };

    Onyx::RenderView<D> *rv = view.Handle;
    EditorScene &scene = s_Editor->Project.GetActiveScene();

    TKit::TierArray<EditorCamera<D>> &cams = scene.GetCameras<D>();
    const TKit::StackArray<TKit::StackString> camLabels = CreateNameArray<EditorCamera<D>>(cams);

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
        const auto swapCameras = [=](const u32 toSet, const u32 toRemove) {
            EditorView<D> *rView = undoRedo_CheckViewExists();
            EditorCamera<D> *ccam = undoRedo_CheckCameraExists(toSet);
            EditorCamera<D> *pcam = undoRedo_CheckCameraExists(toRemove);
            if (rView && ccam && pcam)
            {
                ++ccam->RefCount;
                --pcam->RefCount;
                rView->Handle->SetCamera(ccam->Handle);
            }
        };
        const auto undo = [=] { swapCameras(prev, selected); };
        const auto redo = [=] { swapCameras(selected, prev); };
        s_Editor->UndoRedo.Do(LambdaCommand{undo, redo});
    }

    ov->ColorEditor("Background color", &rv->ClearColor);
    undoRedo_LambdaContinuousWidget([=, col = rv->ClearColor] {
        EditorView<D> *rView = undoRedo_CheckViewExists();
        if (rView)
            rView->Handle->ClearColor = col;
    });

    Onyx::RenderViewFlags flags = rv->GetFlags();
    ov->HorizontalSeparator("Viewport");

    using View = Onyx::RenderView<D>;

    const auto getVp = &View::GetViewport;
    void (View::*setVp)(const Onyx::Viewport &) = &View::SetFixedViewport;

    const auto getSc = &View::GetScissor;
    void (View::*setSc)(const Onyx::Scissor &) = &View::SetFixedScissor;

    const f32 speed = 1.f;

    Onyx::Viewport viewport = (rv->*getVp)();
    Onyx::Scissor scissor = (rv->*getSc)();

    const auto undoRedo_Viewport = [=] {
        undoRedo_LambdaContinuousWidget([=] {
            EditorView<D> *rView = undoRedo_CheckViewExists();
            if (rView)
                (rView->Handle->*setVp)(viewport);
        });
    };
    const auto undoRedo_Scissor = [=] {
        undoRedo_LambdaContinuousWidget([=] {
            EditorView<D> *rView = undoRedo_CheckViewExists();
            if (rView)
                (rView->Handle->*setSc)(scissor);
        });
    };

    bool changed = ov->HorizontalDrag("Position##Viewport", &viewport.Position, speed);
    undoRedo_Viewport();

    changed |= ov->HorizontalDrag("Size##Viewport", &viewport.Size, speed);
    undoRedo_Viewport();

    if (changed)
        (rv->*setVp)(viewport);

    ov->HorizontalSeparator("Scissor");

    changed = ov->HorizontalDrag("Position##Scissor", &scissor.Position, speed);
    undoRedo_Scissor();

    changed |= ov->HorizontalDrag("Size##Scissor", &scissor.Size, speed);
    undoRedo_Scissor();

    if (changed)
        (rv->*setSc)(scissor);

    const auto undoRedo_Flags = [&](const char *name, const Onyx::RenderViewFlags bits) {
        if (ov->CheckBoxFlags(name, &flags, bits))
        {
            const auto swapFlags = [=](const bool undo) {
                EditorView<D> *rView = undoRedo_CheckViewExists();
                if (rView)
                    rView->Handle->SetFlags(undo == bool(flags & bits) ? (flags & ~bits) : (flags | bits));
            };
            const auto undo = [=] { swapFlags(true); };
            const auto redo = [=] { swapFlags(false); };
            s_Editor->UndoRedo.Do(LambdaCommand{undo, redo});
            return true;
        }
        return false;
    };

    changed = undoRedo_Flags("Shadows", Onyx::RenderViewFlag_Shadows);
    changed |= undoRedo_Flags("Outlines", Onyx::RenderViewFlag_Outlines);

    changed |= undoRedo_Flags("Transparency", Onyx::RenderViewFlag_Transparency);
    changed |= undoRedo_Flags("Hidden", Onyx::RenderViewFlag_Hidden);

    if (changed)
        rv->SetFlags(flags);
}
template <Dimension D> static void displayViews(const char *name, EditorViewport &viewport)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    if (ov->PushTree(name, Onyx::OverlayTreeFlag_DrawLines))
    {
        TKit::TierHive<EditorView<D>> &views = viewport.GetViews<D>();

        const NameArray labels = CreateNameArray(views);

        EditorScene &scene = s_Editor->Project.GetActiveScene();
        TKit::TierArray<EditorCamera<D>> &cams = scene.GetCameras<D>();
        if (ov->BeginPopup("Choose camera"))
        {
            u32 camera = TKIT_U32_MAX;

            for (u32 i = 0; i < cams.GetSize(); ++i)
                if (ov->Button({i, cams[i].Name}, Onyx::OverlayButtonFlag_SpanFullWidth))
                    camera = i;

            if (camera < cams.GetSize())
            {
                scene.CreateRenderView<D>(viewport, camera);
                ov->CloseCurrentPopup();
            }

            ov->EndPopup();
        }

        static u32 selected = TKIT_U32_MAX;
        ListBox lb{"Views", &selected, &labels.Names};
        lb.OnAdd = [&] { ov->OpenPopup("Choose camera"); };
        lb.OnRemove = [&] { scene.DestroyRenderView<D>(labels.Ids[selected], viewport); };

        lb.CanAdd = !cams.IsEmpty();
        lb.CannotAddTooltip =
            D == D2 ? "Add a 2D camera to be able to add a view" : "Add a 3D camera to be able to add a view";

        drawListBox(lb);

        const RenderView selectedRv = labels.GetId(selected);
        if (views.Contains(selectedRv))
            displayView(views[selectedRv]);
        ov->PopTree();
    }
}

template <Dimension D> static void displayCamera(EditorCamera<D> &camera)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const char *elements = D == D2 ? "Orthographic#Viewport" : "Orthographic#Viewport#Perspective";

    ov->HorizontalSeparator(camera.Name);
    ov->InputText("Name", &camera.Name, ONYX_NAME_BUF_SIZE);

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
template <Dimension D> static void displayCameras(const char *name)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    if (ov->PushTree(name, Onyx::OverlayTreeFlag_DrawLines))
    {
        EditorScene &scene = s_Editor->Project.GetActiveScene();

        TKit::TierArray<EditorCamera<D>> &cams = scene.GetCameras<D>();
        const TKit::StackArray<TKit::StackString> labels = CreateNameArray<EditorCamera<D>>(cams);
        static u32 selected = TKIT_U32_MAX;

        ListBox lb{"Cameras", &selected, &labels};
        lb.OnAdd = [&] { scene.CreateCamera<D>(); };
        lb.OnRemove = [&] { scene.DestroyCamera<D>(selected); };
        lb.CanRemove = selected < cams.GetSize() && cams[selected].RefCount == 0;
        lb.CannotRemoveTooltip =
            "A view is currently using this camera. Remove the view or change its camera to delete it";
        drawListBox(lb);

        if (selected < cams.GetSize())
            displayCamera(cams[selected]);
        ov->PopTree();
    }
}

template <Dimension D> static void displayContext(EditorContext<D> &rctx)
{
    Onyx::Overlay *ov = s_Editor->Overlay;

    ov->HorizontalSeparator(rctx.Name);
    ov->InputText("Name", &rctx.Name, ONYX_NAME_BUF_SIZE);

    ov->CheckBox("Axes", &rctx.DrawAxes);
    if (ov->PushTree("Targets"))
    {
        Onyx::RenderContext<D> *context = rctx.Handle;
        const EditorScene &scene = s_Editor->Project.GetActiveScene();
        for (const EditorViewport &vp : scene.Viewports)
        {
            ov->HorizontalSeparator(vp.Name);
            const TKit::TierHive<EditorView<D>> &views = vp.GetViews<D>();
            const TKit::StackArray<RenderView> ids = CreateIdArray(views);
            for (const RenderView rv : ids)
            {
                const EditorView<D> &rview = views[rv];
                ov->PushId(&rview);
                ov->CheckBoxFlags(rview.Name, &context->ViewMask, rview.Handle->GetViewBit());
                ov->PopId();
            }
        }
        ov->PopTree();
    }
}

template <Dimension D> static void displayContext(const char *name)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    if (ov->PushTree(name, Onyx::OverlayTreeFlag_DrawLines))
    {
        EditorScene &scene = s_Editor->Project.GetActiveScene();

        TKit::TierHive<EditorContext<D>> &ctxs = scene.GetContexts<D>();
        const NameArray labels = CreateNameArray(ctxs);

        static u32 selected = TKIT_U32_MAX;
        ListBox lb{"Contexts", &selected, &labels.Names};
        lb.OnAdd = [&] { Scene_CreateRenderContext<D>(s_Editor->Project.ActiveScene); };
        lb.OnRemove = [&] { Scene_DestroyRenderContext<D>(s_Editor->Project.ActiveScene, labels.Ids[selected]); };

        drawListBox(lb);

        const RenderContext selectedRc = labels.GetId(selected);
        if (ctxs.Contains(selectedRc))
            displayContext(ctxs[selectedRc]);

        ov->PopTree();
    }
}

static void displayViewport(EditorViewport &viewport)
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    ov->HorizontalSeparator(viewport.Name);
    ov->InputText("Name", &viewport.Name, ONYX_NAME_BUF_SIZE);
    ov->Text("Resolution: {}x{}", viewport.Resolution[0], viewport.Resolution[1]);
    ov->CheckBox("Visible", &viewport.Visible);

    const EditorScene &scene = s_Editor->Project.GetActiveScene();
    if (scene.HasDimension<D2>())
        displayViews<D2>(NAME_BY_DIM("2D", "Views"), viewport);
    if (scene.HasDimension<D3>())
        displayViews<D3>(NAME_BY_DIM("3D", "Views"), viewport);
}

static void displayViewports()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    if (ov->PushTree("Viewports", Onyx::OverlayTreeFlag_DrawLines))
    {
        EditorScene &scene = s_Editor->Project.GetActiveScene();

        const NameArray labels = CreateNameArray(scene.Viewports);
        static u32 selected = TKIT_U32_MAX;

        ListBox lb{"Viewports", &selected, &labels.Names};
        lb.OnAdd = [&] { Scene_CreateViewport(s_Editor->Project.ActiveScene, ONYX_DEFAULT_RESOLUTION); };
        lb.OnRemove = [&] { Scene_DestroyViewport(s_Editor->Project.ActiveScene, labels.Ids[selected]); };
        drawListBox(lb);

        const Viewport selectedVp = labels.GetId(selected);
        if (scene.Viewports.Contains(selectedVp))
            displayViewport(scene.Viewports[selectedVp]);
        ov->PopTree();
    }
}

static void drawSceneWindow()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    if (ov->BeginWindow(ids.Scene))
    {
        EditorScene &scene = s_Editor->Project.GetActiveScene();
        if (scene.Viewports.IsEmpty())
        {
            ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Honey);
            ov->TextRaw("The current scene has no viewports. Create one below to draw something");
            ov->PopStyleColor();
        }

        ov->PushPanel(Onyx::LayoutDirection_LeftToRight);
        ov->RadioButton("2D", &scene.Dim, D2);
        ov->RadioButton("3D", &scene.Dim, D3);
        ov->RadioButton("Both", &scene.Dim, 0);
        ov->PopPanel();

        displayViewports();

        if (scene.HasDimension<D2>())
            displayCameras<D2>(NAME_BY_DIM("2D", "Editor cameras"));
        if (scene.HasDimension<D3>())
            displayCameras<D3>(NAME_BY_DIM("3D", "Editor cameras"));

        if (scene.HasDimension<D2>())
            displayContext<D2>(NAME_BY_DIM("2D", "Render contexts"));
        if (scene.HasDimension<D3>())
            displayContext<D3>(NAME_BY_DIM("3D", "Render contexts"));

        ov->EndWindow();
    }
}

template <Dimension D>
static Onyx::RenderView<D> *getHoveredView(const EditorViewport &viewport, f32v2 *outMpos = nullptr)
{
    Onyx::Window *win = s_Editor->Window;
    const Onyx::RenderTexture *rt = viewport.Target;

    const auto views = rt->GetSortedViews<D>();
    const f32v2 vpScreenPos =
        s_Editor->Overlay->GetMainNativeWindow()->ToLocalScreen(viewport.Position + f32v2{0.f, viewport.Size[1]});

    const f32v2 mpos = win->GetMousePosition() - vpScreenPos;
    for (Onyx::RenderView<D> *rv : views)
    {
        if (rv->IsWithinViewport(mpos))
        {
            if (outMpos)
                *outMpos = mpos;
            return rv;
        }
    }
    return nullptr;
}

template <Dimension D> static void applyCameraMovement()
{
    EditorScene &scene = s_Editor->Project.GetActiveScene();
    for (const EditorViewport &viewport : scene.Viewports)
        if (viewport.Window && viewport.Window->IsHovered())
        {
            Onyx::RenderView<D> *rv = getHoveredView<D>(viewport);
            if (rv)
            {
                Onyx::Window *win = s_Editor->Window;
                const TKit::Timespan dt = Onyx::GetDeltaTime(win);
                win->ControlCamera(dt, rv->GetCamera());
                return;
            }
        }
}

template <Dimension D> static void applyZoom(const f32 scroll)
{
    EditorScene &scene = s_Editor->Project.GetActiveScene();
    for (const EditorViewport &viewport : scene.Viewports)
        if (viewport.Window && viewport.Window->IsHovered())
        {
            f32v2 mpos;
            Onyx::RenderView<D> *rv = getHoveredView<D>(viewport, &mpos);
            if (rv)
            {
                Onyx::Window *win = s_Editor->Window;
                const f32 factor = win->IsKeyPressed(Onyx::Key_LeftShift) ? 0.05f : 0.005f;
                if constexpr (D == D2)
                    rv->ZoomScroll(mpos, factor * scroll);
                else
                    rv->ZoomScroll(f32v3{mpos, 0.5f}, factor * scroll);
                return;
            }
        }
}

static void handleEvents()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    Onyx::Window *win = s_Editor->Window;
    const bool wantKb = ov->WantCaptureKeyboard();
    // const bool wantMouse = ov->WantCaptureMouse();
    const bool wantScroll = ov->WantCaptureScroll();
    if (!wantKb)
    {
        applyCameraMovement<D2>();
        applyCameraMovement<D3>();
    }

    const bool ctrl = win->IsKeyPressed(Onyx::Key_LeftControl);
    for (const Onyx::Event &ev : win->GetNewEvents())
        if (!wantKb && ctrl && ev.Type == Onyx::Event_KeyReleased)
        {
            if (ev.Key == Onyx::Key_Z)
                s_Editor->UndoRedo.Undo();
            else if (ev.Key == Onyx::Key_Y)
                s_Editor->UndoRedo.Redo();
        }
        else if (!wantScroll && ev.Type == Onyx::Event_Scrolled)
        {
            const f32 scroll = ev.ScrollOffset[1];
            applyZoom<D2>(scroll);
            applyZoom<D3>(scroll);
            break;
        }
}

template <Dimension D> static void drawAxes()
{
    EditorScene &scene = s_Editor->Project.GetActiveScene();
    for (const EditorContext<D> &rc : scene.GetContexts<D>())
        if (rc.DrawAxes)
            rc.Handle->Axes();
}

void Run()
{
    Onyx::Overlay *ov = s_Editor->Overlay;
    const Labels &ids = *s_Labels;
    while (Onyx::Running())
    {
        ov->FullScreenDockSpace(ids.MainDockSpace,
                                Onyx::OverlayDockNodeFlag_CanBeEmpty | Onyx::OverlayDockNodeFlag_StartWithTabBarHidden,
                                Onyx::OverlayWindowFlag_NoBackground | Onyx::OverlayWindowFlag_MousePassThrough |
                                    Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);

        drawEditorWindow();
        drawHierarchyWindow();
        drawEntityWindow();
        drawAssetBrowserWindow();
        drawConsoleWindow();
        drawSceneWindow();
        drawViewportWindow();

        handleEvents();

        const Scene active = s_Editor->Project.ActiveScene;
        Scene_FlushContexts(active);
        drawAxes<D2>();
        drawAxes<D3>();
        Scene_Render(active);

        ov->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
}

void Terminate()
{
    // window and overlay are destroyed automatically
    s_Editor.Destruct();
    s_Labels.Destruct();
    Onyx::Terminate();
}

Scene Scene_Create()
{
    return s_Editor->Project.CreateScene();
}
void Scene_Destroy(const Scene sc)
{
    s_Editor->Project.DestroyScene(sc);
}

Viewport Scene_CreateViewport(const Scene sc, const u32v2 &resolution)
{
    return s_Editor->Project.Scenes[sc].CreateViewport(resolution);
}
void Scene_DestroyViewport(const Scene sc, const Viewport vp)
{
    s_Editor->Project.Scenes[sc].DestroyViewport(vp);
}
template <Dimension D> RenderContext Scene_CreateRenderContext(const Scene sc)
{
    return s_Editor->Project.Scenes[sc].CreateRenderContext<D>();
}
template <Dimension D> void Scene_DestroyRenderContext(const Scene sc, const RenderContext ctx)
{
    s_Editor->Project.Scenes[sc].DestroyRenderContext<D>(ctx);
}
void Scene_FlushContexts(const Scene sc)
{
    s_Editor->Project.Scenes[sc].FlushContexts();
}
void Scene_Serialize(const Scene sc, const fs::path &path)
{
    s_Editor->Project.Scenes[sc].Serialize(path);
}

TKit::Registry &Scene_GetRegistry(const Scene sc)
{
    return s_Editor->Project.Scenes[sc].Registry;
}
} // namespace Engine
