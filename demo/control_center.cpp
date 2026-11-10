// This file is AI generated: it was written by Claude Code, an AI coding assistant, starting from the original control
// center and a design brief. Review it like any other contribution before relying on it.
//
// =====================================================================================================================
//  ONYX CONTROL CENTER
// =====================================================================================================================
//
//  Two things in one executable, kept apart:
//    - a showcase of what Onyx does: four scene views and a panel per topic, with its controls and live measurements.
//      The showcase never judges anything by itself
//    - the place to test Onyx, on demand, from the Tests window:
//        Automated       runs the onyx-tests suite (tests/), the same executable CI runs, as a child process. Pick
//                        tests, run them, read the failures and copy a report
//        Visual review   what a program cannot judge (is it crisp? does it blend right?). Each check says what to do
//                        in the showcase and what to look at, and you give the verdict
//
//  First run
//    "Where do I start?" opens on launch and points to the Tests window. Verdicts are saved in the working directory
//    (onyx-control-center-verdicts.txt), so a second session resumes where the first one stopped
//
//  Badges (hover a test or a check to read what it is about)
//    PASS / FAIL / CRASH / TIMEOUT / SKIP / KNOWN    automated tests. KNOWN: fails because of a reported bug
//    TODO / GOOD / BAD / SKIP                        visual checks: waiting / judged right / judged broken / skipped
//
//  Layouts, switchable at runtime from the Control center's Layout menu
//    Direct    the four scene views render straight into window quadrants, the panels float on top
//    Docked    every view renders into a render texture shown by its own dockable panel
//
//  Scene views. Each one has a tint, also used to tag the visual checks that concern it
//    Pixel view    CameraMode_Viewport, 1 world unit = 1 logical pixel. DPI and rasterization
//    2D gallery    orthographic camera. 2D primitives, outlines, blending, 2D lights and occluders
//    3D gallery    perspective camera. 3D primitives, shading, shadows, point and spot lights
//    Textures      render textures drawn in the scene, plus an offset viewport for the mouse
//    View lab      one more view, off by default, to reconfigure from its panel (direct layout only)
//
//  Controls, over the 2D or the 3D gallery
//    W A S D Q E          move/rotate the camera of the hovered gallery
//    Space, LCtrl, LShift 3D only: up, down, hold to look around
//    Mouse wheel          zoom the hovered gallery (2D: around the cursor, 3D: field of view)
//
//  DPI testing tip (Hyprland): hyprctl keyword monitor <name>,preferred,auto,1.5   (also try 1, 1.25, 2)
//
//  Audio is optional: it is compiled only with ONYX_ENABLE_AUDIO, and every sound is generated in code
//
//  File sections
//     1. Configuration and theme
//     2. Visual review bookkeeping: visual checks, categories and saved verdicts
//     3. UI helpers: text, badges, tooltips, help popups and check rows
//     4. Application state, the automated test runner's included
//     5. Setup: resources, window, views, contexts, render textures, overlay
//     6. Layouts, events and camera control
//     7. Usage meters: what a frame asks of the renderer, against the Initialize specs
//     8. Scene drawing, one function per view
//     9. Audio: procedural sources and spatial orbit
//    10. Overlay panels: visual review, automated tests, report, manual, dashboard, docked galleries, category panels
//    11. Categories: titles, help, visual checks and the functions behind each panel
//    12. Main loop
//
// =====================================================================================================================

#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"
#include "onyx/sanitizer_options.hpp"
#ifdef ONYX_ENABLE_AUDIO
#    include "onyx/audio.hpp"
#    include <atomic>
#endif
#include "graph/platform.hpp"
#include "graph/swap_chain.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/filesystem/file.hpp"
#include "tkit/filesystem/path.hpp"
// what Toolkit has no counterpart for: floor, exp and popcount, pipes to the test runner, the environment, and the
// reader thread with its mutex and flag
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#ifndef ONYX_ENABLE_AUDIO
#    include <atomic>
#endif

using namespace TKit::Alias;
using Onyx::D2;
using Onyx::D3;
namespace Math = TKit::Math;

// the containers this file is written with, named once: nothing below names a Toolkit allocation, so this line is the
// only one to change to move the control center to another (TKit::DynamicAllocation, say). the default is the tier
// allocator Onyx pushes on the main thread, the one its own strings and paths come from. it belongs to the main thread:
// no other thread may fill or free one of these (the test runner's reader thread hands over raw bytes for that reason)
template <typename T> using Allocation = TKit::TierAllocation<T>;

template <typename T> using Array = TKit::Array<T, Allocation<T>>;
using String = TKit::String<Allocation<char>>;
using Path = TKit::Path<Allocation<char>>;

// calls line(TKit::StringView) for every line of a text, without the '\n'. String::Split would do, but it reserves room
// for one string per character before it looks for delimiters: this allocates nothing
template <typename F> static void forEachLine(const TKit::StringView text, F &&line)
{
    usize start = 0;
    for (usize i = 0; i <= text.GetSize(); ++i)
        if (i == text.GetSize() || text[i] == '\n')
        {
            // a text that ends in a newline has no empty last line
            if (i != text.GetSize() || i != start)
                line(TKit::StringView{text.GetData() + start, i - start});
            start = i + 1;
        }
}

// =====================================================================================================================
//  1. CONFIGURATION AND THEME
// =====================================================================================================================

// force a platform backend here if you need to (Graph::Platform_Wayland, Graph::Platform_X11). it matters for DPI work,
// because each backend reports screen coordinates differently
constexpr Graph::Platform DemoPlatform = Graph::Platform_Auto;

// window titles double as their ids: the dock tree and the layout switch refer to them
constexpr const char *ControlCenterTitle = "Control center";
constexpr const char *ManualTitle = "Where do I start?";
constexpr const char *TestsTitle = "Tests";
constexpr u32v2 InitialWindowSize{1600, 900};
constexpr u32v2 RenderTextureSize{160, 160};
// docked galleries resize their textures to their panels. this is only the size before the first panel is known
constexpr u32v2 InitialPaneTextureSize{400, 300};

constexpr f32 Epsilon = 1e-4f;
constexpr f32 TooltipWidth = 380.f;
constexpr f32 PopupWidth = 460.f;

constexpr const char *VerdictFile = "onyx-control-center-verdicts.txt";

// indexed by Graph::Platform
constexpr const char *PlatformNames[] = {"Any", "Win32", "Cocoa", "Wayland", "X11"};

namespace Theme
{
const Onyx::Color Pass{110u, 214u, 140u};
const Onyx::Color Fail{240u, 100u, 100u};
const Onyx::Color Todo{240u, 200u, 90u};
const Onyx::Color Info{120u, 180u, 240u};
const Onyx::Color Accent{185u, 150u, 245u};
const Onyx::Color Dim{150u, 155u, 172u};
const Onyx::Color Text{236u, 236u, 242u};
const Onyx::Color BadgeText{24u, 24u, 32u};

const Onyx::Color ClearRenderTexture{40u, 34u, 60u};
const Onyx::Color ClearLab{30u, 26u, 46u};
const Onyx::Color Grid{48u, 54u, 76u};
const Onyx::Color Cursor{120u, 255u, 160u};

const Onyx::Color Shapes[6] = {
    Onyx::Color_Coral, Onyx::Color_Honey, Onyx::Color_Mint, Onyx::Color_Sky, Onyx::Color_Lavender, Onyx::Color_Peach,
};

// frame time bars go from green (on target) to red (twice the target or worse)
const Onyx::Color FrameTimeStops[3] = {Pass, Todo, Fail};
} // namespace Theme

enum PaneId : u8
{
    Pane_Pixel,
    Pane_Gallery2,
    Pane_Gallery3,
    Pane_Textures,
    Pane_Count,
    Pane_None = Pane_Count, // a check that concerns no view in particular
};

// the four scene views. a dark version of each tint clears the view, the bright one draws its title and brackets and
// tags the checks that concern it, so the eye can go from a check to the scene
struct PaneInfo
{
    const char *Name;
    const char *Title;
    const char *Shows;
    const char *Corner; // where it sits in the direct layout
    f32v2 Quadrant;
    Onyx::Color Tint;
    Onyx::Color Clear;
};
const PaneInfo PaneInfos[Pane_Count] = {
    {"Pixel view", "PIXEL VIEW  1 unit = 1 logical px", "1 world unit is 1 logical pixel: DPI and rasterization",
     "top left", f32v2{0.f, 0.f}, Onyx::Color{96u, 156u, 255u}, Onyx::Color{14u, 20u, 38u}},
    {"2D gallery", "2D GALLERY", "Every 2D primitive, outlines, blending, 2D lights and occluders", "top right",
     f32v2{0.5f, 0.f}, Onyx::Color{64u, 204u, 180u}, Onyx::Color{12u, 30u, 30u}},
    {"3D gallery", "3D GALLERY", "Every 3D primitive, shading, shadows, point and spot lights", "bottom left",
     f32v2{0.f, 0.5f}, Onyx::Color{178u, 132u, 255u}, Onyx::Color{28u, 18u, 40u}},
    {"Textures", "TEXTURES  offset viewport", "Render textures drawn in the scene, in a viewport away from the origin",
     "bottom right", f32v2{0.5f, 0.5f}, Onyx::Color{240u, 178u, 72u}, Onyx::Color{34u, 26u, 12u}},
};
constexpr f32 QuadrantSize = 0.5f;

// =====================================================================================================================
//  2. VISUAL REVIEW BOOKKEEPING
// =====================================================================================================================
//
//  Visual checks (VisualCheck) are what a program cannot judge: is it crisp, does it blend right, does the sound move.
//  They carry what to do, what to look at and a priority for the guided review, and you give the verdict. Everything a
//  program can judge lives in the onyx-tests suite instead, which the Tests window runs on demand (section 10).
//
//  A Category is one showcase panel: what it shows, its controls and measurements, and the visual checks that concern
//  it.

// small fixed size string, so check details can be stored across frames without touching any allocator
struct Line
{
    char Data[208];
    u32 Size = 0;

    TKit::StringView View() const
    {
        return TKit::StringView{Data, Size};
    }
};

template <typename... Args> static Line formatLine(const fmt::format_string<Args...> str, Args &&...args)
{
    Line line;
    const auto result = fmt::format_to_n(line.Data, sizeof(line.Data) - 1, str, std::forward<Args>(args)...);
    line.Size = u32(result.size < sizeof(line.Data) - 1 ? result.size : sizeof(line.Data) - 1);
    line.Data[line.Size] = '\0';
    return line;
}

enum Verdict : u8
{
    Verdict_Unseen,
    Verdict_Good,
    Verdict_Broken,
    Verdict_Skipped,
    Verdict_Count,
};
constexpr const char *VerdictNames[Verdict_Count] = {"todo", "good", "broken", "skipped"};

// some visual checks only make sense in one layout, or with an audio device
enum Requirement : u8
{
    Requires_Nothing,
    Requires_DirectLayout,
    Requires_DockedLayout,
    Requires_Audio,
};
constexpr const char *RequirementNotes[] = {"", "Only in the direct layout.", "Only in the docked layout.",
                                            "Needs an audio output device."};

struct VisualCheck
{
    const char *Name;
    const char *Do;   // what to do and what must hold, in one or two lines. always visible
    const char *More; // the full explanation, on hover
    u32 Priority;     // the guided flow walks the visual checks in increasing priority
    PaneId Pane;
    Requirement Needs;
    Verdict Mark = Verdict_Unseen;
};

enum CategoryId : u8
{
    Category_Dpi,
    Category_Views,
    Category_Lab,
    Category_Rendering,
    Category_Lights,
    Category_Textures,
    Category_Input,
    Category_Audio,
    Category_Resources,
    Category_Platform,
    Category_Count,
};

struct App;

struct Category
{
    const char *Title = "";
    const char *Summary = "";
    const char *Concepts = "";
    void (*Panel)(App &) = nullptr; // measured values and controls
    bool Open = false;

    TKit::StaticArray<VisualCheck, 24> Visuals{};
};

using CategoryArray = TKit::FixedArray<Category, Category_Count>;

// verdicts survive across runs: one "verdict<tab>name" line per judged visual check
static void saveVerdicts(const CategoryArray &categories)
{
    String contents{};
    for (const Category &cat : categories)
        for (const VisualCheck &check : cat.Visuals)
            if (check.Mark != Verdict_Unseen)
                contents += String::Format("{}\t{}\n", VerdictNames[check.Mark], check.Name);
    // best effort: the review goes on without it
    [[maybe_unused]] const auto written = TKit::WriteFile(VerdictFile, TKit::StringView{contents});
}

static u32 loadVerdicts(CategoryArray &categories)
{
    const auto contents = TKit::ReadFile<String>(VerdictFile);
    if (!contents)
        return 0;
    u32 loaded = 0;
    forEachLine(*contents, [&](TKit::StringView line) {
        if (!line.IsEmpty() && line.GetBack() == '\r')
            line = line.SubString(0, line.GetSize() - 1);
        const usize tab = line.Find('\t');
        if (tab == TKit::StringView::npos)
            return;
        const TKit::StringView mark = line.SubString(0, tab);
        const TKit::StringView name = line.SubString(tab + 1);

        u32 verdict = Verdict_Count;
        for (u32 i = 0; i < Verdict_Count; ++i)
            if (mark == VerdictNames[i])
                verdict = i;
        if (verdict == Verdict_Count)
            return;

        for (Category &cat : categories)
            for (VisualCheck &check : cat.Visuals)
                if (name == check.Name)
                {
                    check.Mark = Verdict(verdict);
                    ++loaded;
                }
    });
    return loaded;
}

// =====================================================================================================================
//  3. UI HELPERS
// =====================================================================================================================

constexpr const char *VerdictBadges[Verdict_Count] = {"TODO", "GOOD", "BAD", "SKIP"};
const Onyx::Color VerdictColors[Verdict_Count] = {Theme::Todo, Theme::Pass, Theme::Fail, Theme::Dim};
constexpr const char *VerdictMeanings[Verdict_Count] = {
    "TODO: visual check waiting for your verdict", "GOOD: visual check you judged right",
    "BAD: visual check you judged broken", "SKIP: visual check you chose to skip"};

// sets the raw color and puts it back instead of PushStyleColor/PopStyleColor: the pop writes back the color with the
// current alpha applied, so inside a disabled region every pair fades the text color for good, and every widget title
// with it (overlay.style_colors_survive_a_push_inside_a_disabled_region)
static void text(Onyx::Overlay *ui, const Onyx::Color &color, const TKit::StringView str,
                 const Onyx::LayoutTextMode mode = Onyx::TextMode_Unbounded)
{
    Onyx::Color &textColor = ui->GetStyle().Colors[Onyx::OverlayColor_Text];
    const Onyx::Color old = textColor;
    textColor = color;
    ui->TextRaw(mode, str);
    textColor = old;
}

static void badge(Onyx::Overlay *ui, const char *label, const Onyx::Color &color)
{
    ui->BeginPanel({.FillColor = color,
                    .Alignment = Onyx::Overlay::Center,
                    .Sizing = {Onyx::LayoutSizing::Fit(48.f), Onyx::LayoutSizing::Fit()},
                    .Shape = Onyx::LayoutShape::Rectangle(4.f),
                    .Padding = f32v4{5.f, 5.f, 1.f, 1.f}});
    text(ui, Theme::BadgeText, label);
    ui->EndPanel();
}

// a small square in the tint of the view a check concerns. checks about no view in particular keep the slot empty, so
// names stay aligned
static void swatch(Onyx::Overlay *ui, const PaneId pane)
{
    ui->Panel({.FillColor = pane == Pane_None ? Onyx::Color_Transparent : PaneInfos[pane].Tint,
               .Sizing = Onyx::LayoutSizing::Fixed(f32v2{10.f}),
               .Shape = Onyx::LayoutShape::Rectangle(2.f)});
}

// a measured value: dim label, then the value
static void field(Onyx::Overlay *ui, const char *label, const TKit::StringView value)
{
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    text(ui, Theme::Dim, label);
    text(ui, Theme::Info, value);
    ui->PopPanel();
}

// explanations live in tooltips, so they take no room from the scene. they are drawn in the main view on Wayland,
// where windows are not promoted: NoPromotion keeps them inside the window elsewhere too, and the overlay clamps them
// to its right and bottom edges. the fixed maximum width keeps long ones from stretching
static void tooltip(Onyx::Overlay *ui, const Onyx::Color &color, const char *heading, const TKit::StringView body,
                    const Onyx::OverlayFocusFlags flags = 0)
{
    if (!ui->IsItemHovered(Onyx::OverlayFocusFlag_ShortDelay | flags))
        return;
    ui->BeginTooltip(Onyx::OverlayTooltipFlag_Reset | Onyx::OverlayTooltipFlag_NoPromotion);
    ui->BeginPanel({.Direction = Onyx::LayoutDirection_TopToBottom,
                    .Alignment = Onyx::Overlay::TopLeft,
                    .Sizing = {Onyx::LayoutSizing::Fit(0.f, TooltipWidth), Onyx::LayoutSizing::Fit()},
                    .ChildGap = 4.f});
    text(ui, color, heading, Onyx::TextMode_Wrapped);
    if (!body.IsEmpty())
        text(ui, Theme::Text, body, Onyx::TextMode_Wrapped);
    ui->EndPanel();
    ui->EndTooltip();
}

// long explanations go behind a small help button that opens a popup, read at leisure and closed. the popup id must be
// unique across the whole overlay
static void helpButton(Onyx::Overlay *ui, const TKit::StringView popup, const char *title, const char *body)
{
    if (ui->Button("?##help", Onyx::OverlayButtonFlag_Small))
        ui->OpenPopup(Onyx::LayoutId{popup});
    tooltip(ui, Theme::Accent, title, "Click for the full explanation");
    if (!ui->BeginPopup(popup, Onyx::OverlayWindowFlag_AutoResize))
        return;
    ui->BeginPanel({.Direction = Onyx::LayoutDirection_TopToBottom,
                    .Alignment = Onyx::Overlay::TopLeft,
                    .Sizing = {Onyx::LayoutSizing::Fixed(PopupWidth), Onyx::LayoutSizing::Fit()},
                    .ChildGap = 6.f});
    text(ui, Theme::Accent, title);
    text(ui, Theme::Text, body, Onyx::TextMode_Wrapped);
    if (ui->Button("Close"))
        ui->CloseCurrentPopup();
    ui->EndPanel();
    ui->EndPopup();
}

// returns true when the verdict changed, so it can be saved
static bool drawVisualCheck(Onyx::Overlay *ui, VisualCheck &check, const bool applicable)
{
    ui->PushId(Onyx::LayoutId{&check});
    ui->BeginPanel(ui->IdFromStack("row"), {.Direction = Onyx::LayoutDirection_LeftToRight,
                                            .Alignment = Onyx::Overlay::CenterLeft,
                                            .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                            .ChildGap = ui->GetStyle(Onyx::OverlayStyle_ChildGap)});
    if (applicable)
        badge(ui, VerdictBadges[check.Mark], VerdictColors[check.Mark]);
    else
        badge(ui, "N/A", Theme::Dim);
    swatch(ui, check.Pane);
    text(ui, applicable ? Theme::Text : Theme::Dim, check.Name);
    ui->EndPanel();
    tooltip(ui, applicable ? VerdictColors[check.Mark] : Theme::Dim,
            applicable ? VerdictMeanings[check.Mark] : RequirementNotes[check.Needs], check.More);

    bool changed = false;
    if (applicable)
    {
        text(ui, Theme::Dim, check.Do, Onyx::TextMode_Wrapped);
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
        changed |= ui->RadioButton("To do", &check.Mark, Verdict_Unseen);
        changed |= ui->RadioButton("Right", &check.Mark, Verdict_Good);
        changed |= ui->RadioButton("Broken", &check.Mark, Verdict_Broken);
        changed |= ui->RadioButton("Skip", &check.Mark, Verdict_Skipped);
        ui->PopPanel();
    }
    else
        text(ui, Theme::Dim, RequirementNotes[check.Needs]);
    ui->PopId();
    return changed;
}

// =====================================================================================================================
//  4. APPLICATION STATE
// =====================================================================================================================

enum LayoutMode : u8
{
    Mode_Direct,
    Mode_Docked,
    Mode_Count,
};

// runtime state of one scene view
struct Pane
{
    // the 2D view that draws the title, brackets and cursor ring, one per layout. for the 2D panes it is the view
    // itself. for the 3D gallery it is a transparent 2D view stacked on top of the 3D one
    TKit::FixedArray<Onyx::RenderView<D2> *, Mode_Count> Frame{};
    Onyx::RenderContext<D2> *Context = nullptr; // the context that draws into Frame
    Onyx::RenderTexture *Texture = nullptr;     // what the pane renders into in the docked layout

    // where the overlay drew the docked texture last frame, in logical window pixels, and whether it was hovered
    f32v2 ImagePos{0.f};
    f32v2 ImageSize{0.f};
    bool ImageHovered = false;

    // the mouse in the active frame view's screen space: window pixels in the direct layout, texture pixels when
    // docked. drawing and camera control only use these
    f32v2 Mouse{0.f};
    bool MouseInside = false;
};

struct RenderSettings
{
    bool Outlines = true;
    bool Rotate = true;
    bool Shadows = true;
    bool FXAA = false;
    Onyx::AntiAliasing FXAAMode = Onyx::FXAA_QualityMedium;
    f32 OutlineWidth = 0.08f;
    f32 Alpha = 0.6f;
    f32 Alpha3 = 0.45f; // the translucent shapes in front of the 3D gallery
    Onyx::CircleParameters Circle{.Hollowness = 0.3f, .UpperAngle = 1.6f * Math::Pi<f32>()};
};

// how lights filter their shadow maps. soft shadows (PCSS) take precedence over PCF when both flags are set
enum ShadowFilter : u8
{
    Filter_Hard,
    Filter_PCF,
    Filter_Soft,
    Filter_Count,
};
constexpr Onyx::LightFlags FilterFlags[Filter_Count] = {Onyx::LightFlag_CastShadows,
                                                        Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF,
                                                        Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCSS};
constexpr const char *FilterNames = "Hard#PCF#Soft (PCSS)";

// frame pacing: the target delta time asked for with SetTargetDeltaTime. Onyx sleeps until a window is due, and in
// vsync it resets the target to the monitor's own when the swapchain is recreated or the window moves
enum PacingMode : u8
{
    Pacing_None, // no target: the present mode alone paces frames
    Pacing_Monitor,
    Pacing_Custom,
    Pacing_Count,
};

struct PacingSettings
{
    PacingMode Mode = Pacing_None; // the window opens without a target
    u32 Rate = 30;
    bool Dirty = false;
    TKit::Timespan Requested{};
    // set when onyx replaced the request with the monitor's delta time, which is kept in ResetTo
    bool Reset = false;
    TKit::Timespan ResetTo{};
};

struct LightSettings
{
    bool Lights2 = true;
    bool Directional2 = true;
    f32 DirectionalAngle = Math::Radians(-55.f); // the direction the light travels, in world space
    bool Cone = false;
    bool ConeFollowsMotion = true;
    f32 ConeAngle = 0.f;
    f32 ConeExtent = 0.2f; // half of the cone, as a fraction of half a turn
    f32 ConeDecay = 0.6f;
    bool Sweep = false;
    // soft shadows widen with the light's size: a world size for point lights, an angular one for directional lights
    ShadowFilter Filter2 = Filter_PCF;
    f32 LightSize2 = 0.15f;
    f32 SunAngle2 = Math::Radians(2.f);

    bool Lights3 = true;
    bool SunDown = false; // the 3D directional light points straight down
    bool Point3 = true;
    bool Spot3 = true;
    bool SpotOnly = false;
    bool SpotSwing = true;
    f32 SpotFov = Math::Radians(40.f);
    f32 SpotRange = 8.f;
    f32 SpotTilt = 0.f; // away from straight down
    f32 SpotTurn = 0.f; // which way it tilts, around the vertical axis
    ShadowFilter Filter3 = Filter_PCF;
    f32 PointSize3 = 0.2f;
    f32 SpotSize3 = 0.1f;
    f32 SunAngle3 = Math::Radians(1.f);
};

// what a frame asks of the renderer, per dimension, next to the Initialize maximum each meter is bounded by. the
// first five are the instance buffers, one per geometry and in Onyx::Geometry order
enum UsageMeter : u8
{
    Usage_Circles,
    Usage_StaticMeshes,
    Usage_ParametricMeshes,
    Usage_Glyphs,
    Usage_DynamicMeshInstances,
    Usage_Lights,
    Usage_ShadowMaps, // 2D: one pool for every light type
    Usage_OcclusionMaps,
    Usage_PointShadowMaps, // 3D: one maximum per light type
    Usage_DirectionalShadowMaps,
    Usage_SpotShadowMaps,
    Usage_DynamicVertices,
    Usage_DynamicIndices,
    Usage_DrawCommands,
    Usage_PoolVertices,
    Usage_PoolIndices,
    Usage_DynamicMeshes,
    Usage_Count,
};
struct UsageInfo
{
    const char *Name;
    const char *Spec; // the Onyx::SpecificSpecs field that bounds it
};
constexpr UsageInfo UsageInfos[Usage_Count] = {
    {"Circles ", "MaxInstances"},
    {"Static meshes ", "MaxInstances"},
    {"Parametric meshes ", "MaxInstances"},
    {"Glyphs ", "MaxInstances"},
    {"Dynamic mesh draws ", "MaxInstances"},
    {"Lights, busiest type ", "MaxLightInstances"},
    {"Shadow maps ", "MaxShadowMaps"},
    {"Occlusion maps ", "MaxOcclusionMaps"},
    {"Point light shadow maps ", "MaxShadowMaps[Light_Point]"},
    {"Directional light shadow maps ", "MaxShadowMaps[Light_Directional]"},
    {"Spot light shadow maps ", "MaxShadowMaps[Light_Spot]"},
    {"Dynamic vertices ", "MaxDynamicVertices"},
    {"Dynamic indices ", "MaxDynamicIndices"},
    {"Draw commands (bound) ", "MaxDrawCommands"},
    {"Pool vertices, fullest pool ", "MaxVerticesPerResourcePool"},
    {"Pool indices, fullest pool ", "MaxIndicesPerResourcePool"},
    {"Registered dynamic meshes ", "MaxDynamicMeshes"},
};

struct UsageMeters
{
    TKit::FixedArray<u32, Usage_Count> Now{};
    TKit::FixedArray<u32, Usage_Count> Peak{};
    TKit::FixedArray<u32, Usage_Count> Limit{}; // 0 is a valid maximum: nothing may be used then
    TKit::FixedArray<bool, Usage_Count> Applies{};
};

// how full a meter got, relative to its maximum. anything used against a maximum of 0 is an overflow
static f32 usageFill(const UsageMeters &meters, const u32 meter)
{
    const u32 peak = meters.Peak[meter];
    const u32 limit = meters.Limit[meter];
    if (limit == 0)
        return peak == 0 ? 0.f : TKIT_F32_MAX;
    return f32(peak) / f32(limit);
}

// the view lab: one more 2D view on the window, reconfigured from its panel
enum LabPlacement : u8
{
    LabPlacement_AboveScene, // over the four views, under the overlay
    LabPlacement_BelowScene,
};

struct LabSettings
{
    bool Enabled = false;
    bool FixedViewport = false;
    f32v2 Position{0.3f, 0.25f}; // window fractions (proportional) or logical pixels (fixed)
    f32v2 Size{0.4f, 0.5f};
    f32v2 ScissorPosition{0.f}; // viewport fractions
    f32v2 ScissorSize{1.f};
    f32 ImageScale = 1.f; // image size over viewport size
    bool Outlines = true;
    bool Transparency = true;
    bool FXAA = false;
    Onyx::AntiAliasing FXAAMode = Onyx::FXAA_QualityMedium;
    bool TransparentBackground = false;
    bool Orthographic = true;
    f32 OrthoSize = 6.f;
    f32 Rotation = 0.f;
    f32v2 CameraPosition{0.f};
    LabPlacement Placement = LabPlacement_AboveScene;
};

// the views the Render views panel inspects, in its order
enum InspectedView : u8
{
    Inspect_Pixel,
    Inspect_Gallery2,
    Inspect_Gallery3,
    Inspect_Labels3,
    Inspect_Textures,
    Inspect_Overlay,
    Inspect_RtScaled,
    Inspect_RtFixed,
    Inspect_Lab,
    Inspect_Count,
};

// where the galleries put their moving lights: positions, the 2D light's smoothed velocity and where its cone aims
struct LightRequests
{
    f32v2 Point2{0.f};
    f32v2 Velocity2{0.f};
    f32 ConeAim = 0.f;
    f32v3 Point3{0.f};
    f32v3 SpotPosition{0.f, 4.f, 2.f};
    f32v3 SpotDirection{0.f, -1.f, 0.f};
};

struct InputTracking
{
    TKit::FixedArray<u32, Onyx::Event_Count> EventCounts{};

    // keys and buttons currently held according to the event stream (not according to polling)
    TKit::StaticArray<Graph::Key, 32> HeldKeys{};
    TKit::StaticArray<Graph::Mouse, 8> HeldButtons{};

    const char *LastKey = "None";
    f32v2 LastScroll{0.f};
    f32v2 EventMouse{0.f};
    bool HasEventMouse = false;

    char Typed[64]{};
    u32 TypedSize = 0;
};

struct ScaleEvent
{
    f32 Time;
    f32v2 Scale;
};

#ifdef ONYX_ENABLE_AUDIO
enum Wave : u32
{
    Wave_Sine,
    Wave_Square,
    Wave_Noise,
    Wave_Arpeggio,
    Wave_Beeps,
    Wave_Count,
};
constexpr u32 ScopeSize = 128;
constexpr u32 ScopeStride = 6; // keep one frame out of every 6 for the scope

// a procedural source. the callback runs on the audio thread, so it must not allocate or lock: everything it shares
// with the main thread goes through atomics. the main thread writes the parameters and reads the scope
struct Synth
{
    std::atomic<u32> Shape{Wave_Sine};
    std::atomic<f32> Frequency{220.f};
    std::atomic<u32> Length{0}; // frames until the sound ends, 0 is endless
    std::atomic<u32> Cursor{0}; // frames produced since the start or the last seek
    std::atomic<f32> Phase{0.f};
    std::atomic<u32> Noise{0x9E3779B9u};
    u32 SampleRate = 48000;

    TKit::FixedArray<std::atomic<f32>, ScopeSize> Scope{};
    std::atomic<u32> ScopeHead{0};
};

enum Orbit : u32
{
    Orbit_Off,
    Orbit_2D,
    Orbit_3D,
};

struct AudioState
{
    bool Available = false;

    // the panel's source and a beeping source that orbits the listener
    Synth InstrumentSynth{};
    Synth SpatialSynth{};
    Onyx::Resource Instrument = Onyx::NullHandle;
    Onyx::Resource Spatial = Onyx::NullHandle;

    // settings, applied every frame
    u32 Wave = Wave_Sine;
    f32 Frequency = 220.f;
    f32 Volume = 0.5f;
    f32 Pitch = 1.f;
    f32 Pan = 0.f;
    f32 Master = 1.f;
    bool Loop = false;
    u32 Orbit = Orbit_Off;

    f32v2 Listener2{0.f};
    f32v2 Source2{0.f};
    f32v3 Listener3{0.f};
    f32v3 Source3{0.f};

    u32 InstrumentEnds = 0;
};
#endif

// the automated tests, run through the onyx-tests executable (section 10). Toolkit's dynamic containers live on the
// heap: the thread that reads the runner's output must not touch the per thread tier allocators
enum TestState : u8
{
    Test_NotRun,
    Test_Queued,
    Test_Running,
    Test_Pass,
    Test_Fail,
    Test_Skip,
    Test_Crash,
    Test_Timeout,
    Test_Known,
    Test_StateCount,
};

// one failed check, as Catch2 reports it
struct TestFailure
{
    String Where;      // tests/suites/overlay.cpp:179
    String Expression; // CHECK( presses == 1u )
    String Expansion;  // 0 == 1
    String Info;       // the messages in force, one a line: CHECK_MSG, INFO, the engine's last actions
};

struct TestEntry
{
    String Id;
    String Suite;
    String Description;
    String KnownIssue;
    String Where; // where it is declared
    TestState State = Test_NotRun;
    u32 Milliseconds = 0;
    Array<TestFailure> Failures{};
    String Crash{}; // how its process ended, when it crashed or timed out
    Array<String> Notes{};
    Array<String> Output{}; // the last lines it printed, only for failures
    bool Selected = false;
};

enum TestSpeed : u32
{
    Speed_Fast,
    Speed_Normal,
    Speed_Cinematic,
};
constexpr const char *SpeedNames[] = {"fast", "normal", "cinematic"};

struct TestRunner
{
    Path Executable{};
    String Problem{}; // why the tests cannot be listed or run, if they cannot
    Array<TestEntry> Tests{};
    Array<String> OpenSuites{}; // unfolded in the list

    // no thread, mutex or atomic in Toolkit: the standard ones. the reader thread only moves bytes: it reads what the
    // runner prints into Piped, a buffer of a fixed size, and the main thread takes them out every frame and cuts them
    // into lines. the reader allocates nothing, so it needs no allocator of its own (Toolkit keeps them per thread, and
    // they are not thread safe). it reads Command without copying it: set before the thread starts, left alone until
    // it is joined
    std::thread Reader{};
    std::mutex Mutex{};
    TKit::FixedArray<char, 64 * 1024> Piped; // guarded by Mutex, as PipedSize
    usz PipedSize = 0;
    String Command{};
    String Pending{}; // main thread: what was taken out of Piped and does not end in a newline yet
    std::atomic<bool> Running{false};
    i32 ExitCode = 0;

    u32 Planned = 0;
    u32 Finished = 0;
    f32 StartedAt = 0.f;
    f32 Seconds = 0.f;
    bool HasRun = false;
    String Label{};
    TKit::FixedArray<u32, Test_StateCount> Counts{};

    u32 Jobs = 4;
    u32 Speed = Speed_Fast;
    u32 OutputLines = 120;
    char Filter[64]{};
    bool OnlyProblems = false;

    ~TestRunner()
    {
        // a run still going when the app closes is left behind: its child processes end on their own once the pipe they
        // write to is gone
        if (Reader.joinable())
        {
            if (Running.load())
                Reader.detach();
            else
                Reader.join();
        }
    }
};

struct App
{
    Onyx::Window *Window = nullptr;
    Onyx::Overlay *Ui = nullptr;

    // cameras live here, render views only keep a pointer to them. a pane's window and texture views share its camera
    Onyx::Camera<D2> PixelCam{};
    Onyx::Camera<D2> GalleryCam{};
    Onyx::Camera<D2> LabelCam{};
    Onyx::Camera<D2> TexturesCam{};
    Onyx::Camera<D2> RtCam{};
    Onyx::Camera<D2> LabCam{};
    Onyx::Camera<D3> Cam3{};

    TKit::FixedArray<Pane, Pane_Count> Panes{};
    TKit::FixedArray<Onyx::RenderView<D3> *, Mode_Count> View3{};
    Onyx::RenderView<D2> *RtScaledView = nullptr;
    Onyx::RenderView<D2> *RtFixedView = nullptr;
    Onyx::RenderView<D2> *LabView = nullptr;

    Onyx::RenderContext<D2> *PixelCtx = nullptr;
    Onyx::RenderContext<D2> *GalleryCtx = nullptr;
    Onyx::RenderContext<D3> *Ctx3 = nullptr;
    Onyx::RenderContext<D2> *LabelCtx = nullptr;
    Onyx::RenderContext<D2> *TexturesCtx = nullptr;
    Onyx::RenderContext<D2> *RtCtx = nullptr;
    Onyx::RenderContext<D2> *LabCtx = nullptr;

    Onyx::RenderTexture *RtScaled = nullptr;
    Onyx::RenderTexture *RtFixed = nullptr;

    Onyx::Resource Lit2 = Onyx::NullHandle;
    Onyx::Resource Occluder2 = Onyx::NullHandle;
    Onyx::Resource Mat3 = Onyx::NullHandle;

    RenderSettings Render{};
    LightSettings Lights{};
    LabSettings Lab{};
    LightRequests Requested{};
    InputTracking Input{};
#ifdef ONYX_ENABLE_AUDIO
    AudioState Audio{};
#endif

    // time
    f32 Time = 0.f;
    f32 DeltaTime = 0.f;
    TKit::FixedArray<f32, 96> FrameTimes{};
    u32 FrameTimeHead = 0;

    // dpi and swapchain tracking
    TKit::StaticArray<ScaleEvent, 10> ScaleEvents{};
    u32 SwapchainRecreations = 0;

    // window properties set from the panels
    char Title[128]{};
    u32 PresentMode = Onyx::PresentMode_VSync;
    PacingSettings Pacing{};
    f32 Opacity = 1.f;
    u32v2 RequestedSize = InitialWindowSize;

    // present mode cycle: steps left, when the next one is due and the mode to go back to
    u32 PresentCycleSteps = 0;
    f32 PresentCycleAt = 0.f;
    u32 PresentCycleRestore = Onyx::PresentMode_VSync;

    // layouts. every docked session gets a fresh dockspace, and going back to the direct layout puts the panels back
    // in place: the first frame undocks them, the second places them
    LayoutMode Mode = Mode_Direct;
    LayoutMode RequestedMode = Mode_Direct;
    u32 DockSessions = 0;
    bool DockTreePending = false;
    u32 PlacePanels = 0;

    // ui state
    bool ShowManual = true;
    bool ShowTests = false;
    bool ShowOverlayDemo = false;
    // how many frames each inspected view rendered with each of its framebuffers
    TKit::FixedArray<TKit::FixedArray<u32, 4>, Inspect_Count> FramebufferUse{};
    bool LabMouseInside = false;  // over the part of the lab view its scissor lets through
    PaneId FocusPane = Pane_None; // the view the visual review points at, by highlighting its brackets
    f32 FocusSince = -100.f;      // when it started pointing there
    bool ReviewTabOpen = false;   // the visual review tab is the open one (last frame)
    bool VerdictsDirty = false;
    u32 LoadedVerdicts = 0;
    f32 ReportCopiedAt = -100.f;
    u32 ReportLines = 0;

    CategoryArray Categories{};
    TestRunner Tests{};

    // renderer usage per dimension, measured every frame right before the transfer
    TKit::FixedArray<UsageMeters, 2> Usage{};
};

// =====================================================================================================================
//  5. SETUP
// =====================================================================================================================

static void resetCameras(App &app)
{
    app.GalleryCam.View = {};
    app.GalleryCam.OrthoParameters.Size = 10.f;
    app.Cam3.View = {};
    app.Cam3.View.Translation = f32v3{0.f, 1.5f, 9.5f};
    app.Cam3.Mode = Onyx::CameraMode_Perspective;
    app.Cam3.PerspParameters.FieldOfView = Math::Radians(75.f);
    app.Cam3.PerspParameters.Near = 0.1f;
    app.Cam3.PerspParameters.Far = 100.f;
    // orthographic near and far are view space depths around the camera. the defaults (-5 to 5) would clip the gallery,
    // which sits about 10 units in front of the camera. the size frames it about like the perspective camera does
    app.Cam3.OrthoParameters.Size = 14.f;
    app.Cam3.OrthoParameters.Near = -10.f;
    app.Cam3.OrthoParameters.Far = 50.f;
}

static void setup(App &app)
{
    // -- resources --------------------------------------------------------------------------------------------------
    // materials are registered up front and synced once. Resources_Sync uploads everything registered so far
    app.Lit2 = Onyx::Material_Register<D2>();
    app.Occluder2 = Onyx::Material_Register<D2>({.Occluder = true});
    app.Mat3 = Onyx::Material_Register<D3>({.RoughnessFactor = 0.35f});
    Onyx::Resources_Sync(Onyx::SyncFlag_Materials);

    // -- window -----------------------------------------------------------------------------------------------------
    app.Window = Onyx::OpenWindow({.Window = {.Title = "Onyx control center",
                                              .ScreenSize = InitialWindowSize,
                                              .PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Window *win = app.Window;

    const Graph::PlatformBackendInfo pinfo = Graph::GetPlatformBackendInfo();
    const Line title = formatLine("Onyx control center | {}", PlatformNames[pinfo.Platform]);
    for (u32 i = 0; i <= title.Size && i < sizeof(app.Title) - 1; ++i)
        app.Title[i] = title.Data[i];
    win->SetTitle(app.Title);

    // -- render textures --------------------------------------------------------------------------------------------
    // one follows the window content scale (updated on Event_ContentScaleChanged), the other stays at 1 as a reference.
    // render textures render in creation order, and a texture is only ready to be sampled once its own turn has come.
    // the docked Textures pane samples these two, so they must be created before the pane textures
    app.RtScaled = Onyx::CreateRenderTexture(RenderTextureSize, {.ContentScale = win->GetContentScale()[0]});
    app.RtFixed = Onyx::CreateRenderTexture(RenderTextureSize, {.ContentScale = 1.f});
    app.RtCam.Mode = Onyx::CameraMode_Viewport;
    app.RtScaledView = app.RtScaled->CreateRenderView<D2>(&app.RtCam);
    app.RtFixedView = app.RtFixed->CreateRenderView<D2>(&app.RtCam);
    app.RtScaledView->ClearColor = Theme::ClearRenderTexture;
    app.RtFixedView->ClearColor = Theme::ClearRenderTexture;

    // -- scene views ------------------------------------------------------------------------------------------------
    // every pane has two sets of views on the same camera: one in its window quadrant for the direct layout and one
    // covering a render texture for the docked layout. only the active set is targeted and visible
    app.PixelCam.Mode = Onyx::CameraMode_Viewport;
    app.LabelCam.Mode = Onyx::CameraMode_Viewport;
    app.TexturesCam.Mode = Onyx::CameraMode_Viewport;
    resetCameras(app);

    const Onyx::RenderViewFlags galleryFlags =
        Onyx::RenderViewFlag_Shadows | Onyx::RenderViewFlag_Outlines | Onyx::RenderViewFlag_Transparency;
    for (Pane &pane : app.Panes)
        pane.Texture = Onyx::CreateRenderTexture(InitialPaneTextureSize, {.ContentScale = win->GetContentScale()[0]});

    for (u32 m = 0; m < Mode_Count; ++m)
    {
        TKit::FixedArray<Onyx::RenderTarget *, Pane_Count> targets{};
        for (u32 i = 0; i < Pane_Count; ++i)
            targets[i] = m == Mode_Direct ? static_cast<Onyx::RenderTarget *>(win) : app.Panes[i].Texture;

        app.Panes[Pane_Pixel].Frame[m] =
            targets[Pane_Pixel]->CreateRenderView<D2>(&app.PixelCam, {.Flags = Onyx::RenderViewFlag_Outlines});
        app.Panes[Pane_Gallery2].Frame[m] =
            targets[Pane_Gallery2]->CreateRenderView<D2>(&app.GalleryCam, {.Flags = galleryFlags});
        app.View3[m] = targets[Pane_Gallery3]->CreateRenderView<D3>(&app.Cam3, {.Flags = galleryFlags});
        // 2D views can sit on top of 3D ones: this transparent view shares the 3D gallery's viewport and draws its
        // title, corner brackets and cursor. it only shows if 2D and 3D views are composited by layer. the clear color
        // is transparent black, like the overlay's
        app.Panes[Pane_Gallery3].Frame[m] = targets[Pane_Gallery3]->CreateRenderView<D2>(&app.LabelCam);
        app.Panes[Pane_Textures].Frame[m] = targets[Pane_Textures]->CreateRenderView<D2>(&app.TexturesCam);

        app.View3[m]->ClearColor = PaneInfos[Pane_Gallery3].Clear;
        for (u32 i = 0; i < Pane_Count; ++i)
            app.Panes[i].Frame[m]->ClearColor = PaneInfos[i].Clear;
        app.Panes[Pane_Gallery3].Frame[m]->ClearColor = Onyx::Color{0.f, 0.f, 0.f, 0.f};
    }

    // window views use proportional viewports, so they follow the window when it is resized or its scale changes.
    // texture views keep the default: the whole texture
    for (u32 i = 0; i < Pane_Count; ++i)
        app.Panes[i].Frame[Mode_Direct]->SetProportionalViewport(
            {.Position = PaneInfos[i].Quadrant, .Size = f32v2{QuadrantSize}});
    app.View3[Mode_Direct]->SetProportionalViewport(
        {.Position = PaneInfos[Pane_Gallery3].Quadrant, .Size = f32v2{QuadrantSize}});

    // -- view lab ---------------------------------------------------------------------------------------------------
    // one more window view, created after the scene views so it starts above them. it stays hidden until its panel
    // turns it on, and updateLab() applies its settings every frame
    app.LabView = win->CreateRenderView<D2>(&app.LabCam, {.Flags = Onyx::RenderViewFlag_Hidden});

    // -- render contexts --------------------------------------------------------------------------------------------
    // a context records draw commands and can target any number of views, of any render target. the scene contexts
    // get their targets from setLayout()
    app.PixelCtx = Onyx::CreateRenderContext<D2>();
    app.GalleryCtx = Onyx::CreateRenderContext<D2>();
    app.Ctx3 = Onyx::CreateRenderContext<D3>();
    app.LabelCtx = Onyx::CreateRenderContext<D2>();
    app.TexturesCtx = Onyx::CreateRenderContext<D2>();
    app.Panes[Pane_Pixel].Context = app.PixelCtx;
    app.Panes[Pane_Gallery2].Context = app.GalleryCtx;
    app.Panes[Pane_Gallery3].Context = app.LabelCtx;
    app.Panes[Pane_Textures].Context = app.TexturesCtx;

    app.RtCtx = Onyx::CreateRenderContext<D2>();
    app.RtCtx->AddTarget(app.RtScaledView);
    app.RtCtx->AddTarget(app.RtFixedView);
    app.LabCtx = Onyx::CreateRenderContext<D2>();
    app.LabCtx->AddTarget(app.LabView);

    // -- overlay ----------------------------------------------------------------------------------------------------
    // created last and explicitly brought to the top, so it is drawn above every scene view. docking is off unless
    // asked for at creation
    app.Ui = win->CreateOverlay({.Flags = Onyx::OverlayFlag_Docking});
    win->BringToTop(app.Ui->GetMainNativeWindow()->View);
}

// =====================================================================================================================
//  6. LAYOUTS, EVENTS AND CAMERA CONTROL
// =====================================================================================================================

// every window the docked layout docks. a window still docked cannot join a new tree (asserted), and leaving the docked
// layout must undock them: a window docked in a child dockspace that is no longer submitted bails out of Begin() every
// frame, so it would never show again. undocking happens at the end of the frame
static void undockPanels(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    for (const Category &cat : app.Categories)
        ui->UndockWindow(Onyx::LayoutId{cat.Title});
    ui->UndockWindow(Onyx::LayoutId{ControlCenterTitle});
    ui->UndockWindow(Onyx::LayoutId{ManualTitle});
    ui->UndockWindow(Onyx::LayoutId{TestsTitle});
    for (const PaneInfo &info : PaneInfos)
        ui->UndockWindow(Onyx::LayoutId{info.Name});
}

// moves every scene context to the views of the requested layout. hidden views neither render nor composite. a
// context is never left targeting both sets: every shadow casting light takes one shadow map per targeted view, and
// the default specs have just enough of them
static void setLayout(App &app, const LayoutMode mode)
{
    const LayoutMode old = mode == Mode_Direct ? Mode_Docked : Mode_Direct;
    for (Pane &pane : app.Panes)
    {
        pane.Context->RemoveTarget(pane.Frame[old]);
        pane.Context->AddTarget(pane.Frame[mode]);
        pane.Frame[old]->AddFlags(Onyx::RenderViewFlag_Hidden);
        pane.Frame[mode]->RemoveFlags(Onyx::RenderViewFlag_Hidden);
        pane.ImageHovered = false;
        pane.ImageSize = f32v2{0.f};
    }
    app.Ctx3->RemoveTarget(app.View3[old]);
    app.Ctx3->AddTarget(app.View3[mode]);
    app.View3[old]->AddFlags(Onyx::RenderViewFlag_Hidden);
    app.View3[mode]->RemoveFlags(Onyx::RenderViewFlag_Hidden);

    app.Mode = mode;
    app.RequestedMode = mode;
    app.FramebufferUse = {};
    if (mode == Mode_Docked)
    {
        ++app.DockSessions;
        app.DockTreePending = true;
    }
    else
    {
        undockPanels(app);
        app.PlacePanels = 2;
    }
}

// docked textures follow the logical size of their panels at the window's content scale, and the mouse is mapped from
// the panel's image into the texture. the image rectangle and its hover state come from last frame's overlay pass
static void updatePanes(App &app)
{
    const f32v2 mouse = app.Window->GetMousePosition();
    const f32v2 scale = app.Window->GetContentScale();

    // the lab view only shows through its scissor. above the scene, that part hides the panes under it
    const Onyx::RenderView<D2> *lab = app.LabView;
    const f32v2 inScissor = lab->ScreenToViewport(mouse) - lab->GetScissor().Position;
    const f32v2 &scissorSize = lab->GetScissor().Size;
    app.LabMouseInside = !(lab->GetFlags() & Onyx::RenderViewFlag_Hidden) && lab->IsWithinViewport(mouse) &&
                         inScissor[0] >= 0.f && inScissor[1] >= 0.f && inScissor[0] < scissorSize[0] &&
                         inScissor[1] < scissorSize[1];
    const bool labCovers = app.LabMouseInside && app.Lab.Placement == LabPlacement_AboveScene;

    for (Pane &pane : app.Panes)
    {
        if (app.Mode == Mode_Direct)
        {
            pane.Mouse = mouse;
            pane.MouseInside = !labCovers && pane.Frame[Mode_Direct]->IsWithinViewport(mouse);
            continue;
        }

        Onyx::RenderTexture *rt = pane.Texture;
        const u32v2 size{Math::Round(pane.ImageSize)};
        if (size[0] > 0 && size[1] > 0 &&
            (size != rt->GetSize() || !Math::Approximately(rt->GetContentScale(), scale, Epsilon)))
            rt->Resize(size, scale);

        // the image may be stretched for a frame while the texture catches up, so map through the image size
        const bool hasImage = pane.ImageSize[0] >= 1.f && pane.ImageSize[1] >= 1.f;
        pane.Mouse = hasImage ? (mouse - pane.ImagePos) * f32v2{rt->GetSize()} / pane.ImageSize : f32v2{-1.f};
        pane.MouseInside = hasImage && pane.ImageHovered;
    }
}

template <typename Array, typename T> static bool removeHeld(Array &held, const T value)
{
    for (T *it = held.begin(); it != held.end(); ++it)
        if (*it == value)
        {
            held.RemoveUnordered(it);
            return true;
        }
    return false;
}

static void processEvents(App &app)
{
    InputTracking &in = app.Input;
    Onyx::Window *win = app.Window;
    Pane &gallery2 = app.Panes[Pane_Gallery2];
    Pane &gallery3 = app.Panes[Pane_Gallery3];

    for (const Onyx::Event &event : win->GetNewEvents())
    {
        ++in.EventCounts[event.Type];
        switch (event.Type)
        {
        case Onyx::Event_KeyPressed:
            in.LastKey = Graph::ToString(event.Key);
            if (!in.HeldKeys.Contains(event.Key) && !in.HeldKeys.IsFull())
                in.HeldKeys.Append(event.Key);
            break;
        case Onyx::Event_KeyReleased:
            removeHeld(in.HeldKeys, event.Key);
            break;
        case Onyx::Event_MouseMoved:
            in.EventMouse = event.Mouse.Position;
            in.HasEventMouse = true;
            break;
        case Onyx::Event_MousePressed:
            if (!in.HeldButtons.Contains(event.Mouse.Button) && !in.HeldButtons.IsFull())
                in.HeldButtons.Append(event.Mouse.Button);
            break;
        case Onyx::Event_MouseReleased:
            removeHeld(in.HeldButtons, event.Mouse.Button);
            break;
        case Onyx::Event_Scrolled:
            in.LastScroll = event.ScrollOffset;
            // docked galleries live inside overlay windows, so only the direct layout defers to the overlay here. a
            // docked pane only counts as hovered when its image is the topmost thing under the mouse
            if (app.Mode == Mode_Direct && app.Ui->WantCaptureScroll())
                break;
            if (gallery2.MouseInside)
                gallery2.Frame[app.Mode]->ZoomScroll(gallery2.Mouse, 0.1f * event.ScrollOffset[1]);
            else if (gallery3.MouseInside)
                app.View3[app.Mode]->ZoomScroll(f32v3{gallery3.Mouse, 0.f}, 0.05f * event.ScrollOffset[1]);
            break;
        case Onyx::Event_ContentScaleChanged:
            if (app.ScaleEvents.IsFull())
                app.ScaleEvents.RemoveOrdered(app.ScaleEvents.begin());
            app.ScaleEvents.Append(ScaleEvent{app.Time, event.ContentScale});
            app.RtScaled->SetContentScale(event.ContentScale);
            break;
        case Onyx::Event_CharInput:
            if (event.Character >= 32 && event.Character < 127)
            {
                if (in.TypedSize == sizeof(in.Typed) - 1)
                {
                    for (u32 i = 1; i < in.TypedSize; ++i)
                        in.Typed[i - 1] = in.Typed[i];
                    --in.TypedSize;
                }
                in.Typed[in.TypedSize++] = char(event.Character);
                in.Typed[in.TypedSize] = '\0';
            }
            break;
        case Onyx::Event_SwapchainRecreated:
            ++app.SwapchainRecreations;
            break;
        default:
            break;
        }
    }
}

// only the hovered gallery's camera moves, and never while the overlay wants the keyboard. note that
// Window::GetMouseRenderView<D2>() is not usable here: the overlay's own view is a 2D view covering the whole window,
// and it is always on top. in the docked layout the mouse is always over an overlay window, so the mouse capture is
// ignored there and the image hover decides
static void controlCameras(App &app)
{
    const bool overlayMouse = app.Mode == Mode_Direct && app.Ui->WantCaptureMouse();
    if (app.Ui->WantCaptureKeyboard() || overlayMouse)
        return;
    const TKit::Timespan dt = Onyx::GetDeltaTime(app.Window);
    if (app.Panes[Pane_Gallery2].MouseInside)
        app.Window->ControlCamera(dt, &app.GalleryCam);
    else if (app.Panes[Pane_Gallery3].MouseInside)
        app.Window->ControlCamera(dt, &app.Cam3);
}

// the lab's settings are applied every frame. in the docked layout the dockspace covers the window, so the lab is
// hidden there
static void updateLab(App &app)
{
    const LabSettings &lab = app.Lab;
    Onyx::RenderView<D2> *rv = app.LabView;

    Onyx::RenderViewFlags flags = lab.Enabled && app.Mode == Mode_Direct ? 0 : Onyx::RenderViewFlag_Hidden;
    if (lab.Outlines)
        flags |= Onyx::RenderViewFlag_Outlines;
    if (lab.Transparency)
        flags |= Onyx::RenderViewFlag_Transparency;
    if (lab.FXAA)
        flags |= Onyx::RenderViewFlag_FXAA;
    if (flags != rv->GetFlags())
        rv->SetFlags(flags);
    rv->AntiAliasingMode = lab.FXAAMode;

    if (lab.FixedViewport)
        rv->SetFixedViewport({.Position = lab.Position, .Size = lab.Size});
    else
        rv->SetProportionalViewport({.Position = lab.Position, .Size = lab.Size});
    rv->SetProportionalScissor({.Position = lab.ScissorPosition, .Size = lab.ScissorSize});
    rv->SetProportionalImageSize(f32v2{lab.ImageScale});
    rv->ClearColor = lab.TransparentBackground ? Onyx::Color{0.f, 0.f, 0.f, 0.f} : Theme::ClearLab;

    app.LabCam.Mode = lab.Orthographic ? Onyx::CameraMode_Orthographic : Onyx::CameraMode_Viewport;
    app.LabCam.OrthoParameters.Size = lab.OrthoSize;
    app.LabCam.View.Translation = lab.CameraPosition;
    app.LabCam.View.Rotation = lab.Rotation;
}

// requests from the platform panel apply right before Render, which then scans the same events as below and resets
// the target to the monitor's in vsync. mirroring it here lets the panel say when the request was replaced
static void updatePacing(App &app)
{
    PacingSettings &pc = app.Pacing;
    Onyx::Window *win = app.Window;
    const TKit::Timespan monitor = win->GetMonitorDeltaTime();
    if (pc.Mode == Pacing_Monitor && pc.Requested != monitor) // follows the monitor the window is on
        pc.Dirty = true;
    if (pc.Dirty)
    {
        pc.Requested = pc.Mode == Pacing_None      ? TKit::Timespan{}
                       : pc.Mode == Pacing_Monitor ? monitor
                                                   : Onyx::ToDeltaTime(pc.Rate);
        Onyx::SetTargetDeltaTime(win, pc.Requested);
        pc.Reset = false;
        pc.Dirty = false;
    }
    if (win->GetPresentMode() == Onyx::PresentMode_VSync)
        for (const Onyx::Event &event : win->GetNewEvents())
            if (event.Type == Onyx::Event_SwapchainRecreated || event.Type == Onyx::Event_WindowMoved)
            {
                pc.Reset = true;
                pc.ResetTo = monitor;
            }
}

// steps the present mode cycle started from the Platform and device panel
static void updatePresentCycle(App &app)
{
    if (app.PresentCycleSteps == 0 || app.Time < app.PresentCycleAt)
        return;
    --app.PresentCycleSteps;
    app.PresentMode =
        app.PresentCycleSteps == 0 ? app.PresentCycleRestore : (app.PresentMode + 1) % u32(Graph::PresentMode_Count);
    app.Window->SetPresentMode(Graph::PresentMode(app.PresentMode));
    app.PresentCycleAt = app.Time + 0.75f;
}

// =====================================================================================================================
//  7. USAGE METERS
// =====================================================================================================================

// Initialize sizes every per frame renderer buffer from Onyx::Specs, and a frame that overflows one asserts. what a
// frame asks for is measured here from every context, the way Renderer_Transfer copies it. draw commands are an upper
// bound: hidden views count, and every 3D directional light is assumed to render all its cascades in every view
template <Onyx::Dimension D> struct UsageTally
{
    TKit::FixedArray<u32, Onyx::Geometry_Count> Instances{};
    TKit::FixedArray<u32, Onyx::LightTypeCount<D>> Lights{};
    TKit::FixedArray<u32, Onyx::LightTypeCount<D>> ShadowMaps{}; // 2D has a single pool, in the first slot
    u32 DynamicVertices = 0;
    u32 DynamicIndices = 0;
    // per view: commands of its own passes, shaded ranges (drawn again by every shadow map render) and shadow map
    // renders, split into the plain (circles) and indexed (meshes) command buffers
    TKit::FixedArray<u32, ONYX_MAX_VIEWS> CircleCommands{};
    TKit::FixedArray<u32, ONYX_MAX_VIEWS> MeshCommands{};
    TKit::FixedArray<u32, ONYX_MAX_VIEWS> ShadedCircles{};
    TKit::FixedArray<u32, ONYX_MAX_VIEWS> ShadedMeshes{};
    TKit::FixedArray<u32, ONYX_MAX_VIEWS> ShadowRenders{};
};

template <Onyx::Dimension D> static void tallyContext(UsageTally<D> &tally, const Onyx::RenderContext<D> &ctx)
{
    constexpr Onyx::Geometry poolGeometries[Onyx::Resource_MeshPoolCount] = {
        Onyx::Geometry_Static, Onyx::Geometry_Parametric, Onyx::Geometry_Glyph};
    const Onyx::ContextInstanceData &data = ctx.GetInstanceData();

    // every non empty range is one draw command per pipeline pass it uses, in every view the context targets
    u32 circleCommands = 0;
    u32 meshCommands = 0;
    u32 shadedCircles = 0;
    u32 shadedMeshes = 0;
    const auto addRange = [&](const Onyx::Geometry geo, const u32 instances, const u32 mode) {
        if (instances == 0)
            return;
        tally.Instances[geo] += instances;
        const Onyx::RenderModeFlags flags = Onyx::GetRenderModeFlags(Onyx::RenderMode(mode));
        const bool shaded = flags & Onyx::RenderModeFlag_Shaded;
        const u32 passes = u32(shaded) + u32((flags & Onyx::RenderModeFlag_Flat) != 0) +
                           u32((flags & Onyx::RenderModeFlag_Outlined) != 0);
        if (geo == Onyx::Geometry_Circle)
        {
            circleCommands += passes;
            shadedCircles += shaded;
        }
        else
        {
            meshCommands += passes;
            shadedMeshes += shaded;
        }
    };
    for (u32 blend = 0; blend < Onyx::BlendPass_Count; ++blend)
        for (u32 mode = 0; mode < Onyx::RenderMode_Count; ++mode)
        {
            addRange(Onyx::Geometry_Circle, data.Circles[blend][mode].Instances, mode);

            // a dynamic mesh's vertices and indices are copied again for every range that draws it
            const Onyx::InstanceResourceGroup &dynamic = data.DynamicMeshes[blend][mode];
            for (const u32 rid : dynamic.Registry.ResourceIds)
            {
                const u32 instances = dynamic.Buffers[rid].Instances;
                addRange(Onyx::Geometry_Dynamic, instances, mode);
                if (instances == 0)
                    continue;
                const Onyx::DynamicMeshData<D> &mesh =
                    Onyx::DynamicMesh_GetData<D>(Onyx::Handle_CreateForResource(Onyx::Resource_DynamicMesh, rid));
                tally.DynamicVertices += mesh.Vertices.GetSize();
                tally.DynamicIndices += mesh.Indices.GetSize();
            }

            for (u32 type = 0; type < Onyx::Resource_MeshPoolCount; ++type)
                for (const u32 pid : Onyx::ResourcePool_GetIds<D>(Onyx::ResourceType(type)))
                {
                    const Onyx::InstanceResourceGroup &meshes = data.Meshes[blend][mode][type][pid];
                    for (const u32 rid : meshes.Registry.ResourceIds)
                        addRange(poolGeometries[type], meshes.Buffers[rid].Instances, mode);
                }
        }

    // a shadow casting light takes one shadow map per targeted view, and renders it once per cube face or cascade
    const u32 views = u32(std::popcount(ctx.ViewMask));
    u32 renders = 0;
    const auto addLights = [&]<typename Params>(const Onyx::LightType type, const TKit::TierArray<Params> &lights) {
        tally.Lights[type] += lights.GetSize();
        for (const Params &light : lights)
        {
            if (!(light.Flags & Onyx::LightFlag_CastShadows))
                continue;
            tally.ShadowMaps[D == D2 ? 0 : type] += views;
            if constexpr (std::is_same_v<Params, Onyx::PointLightParameters<D3>>)
                renders += 6;
            else if constexpr (std::is_same_v<Params, Onyx::DirectionalLightParameters<D3>>)
                renders += light.Cascades.Count;
            else
                ++renders;
        }
    };
    addLights(Onyx::Light_Point, ctx.GetPointLightParams());
    addLights(Onyx::Light_Directional, ctx.GetDirectionalLightParams());
    if constexpr (D == D3)
        addLights(Onyx::Light_Spot, ctx.GetSpotLightParams());

    for (u32 view = 0; view < ONYX_MAX_VIEWS; ++view)
        if (ctx.ViewMask & (Onyx::ViewMask(1) << view))
        {
            tally.CircleCommands[view] += circleCommands;
            tally.MeshCommands[view] += meshCommands;
            tally.ShadedCircles[view] += shadedCircles;
            tally.ShadedMeshes[view] += shadedMeshes;
            tally.ShadowRenders[view] += renders;
        }
}

// the fill of the fullest default mesh pool, in vertices and indices. this demo registers no meshes of its own, and
// the default font lives in a 2D pool
static void addPoolFill(u32 &vertices, u32 &indices, const Onyx::MeshRange &range)
{
    vertices = Math::Max(vertices, range.VertexOffset + range.VertexCount);
    indices = Math::Max(indices, range.IndexOffset + range.IndexCount);
}

template <Onyx::Dimension D> static void measurePools(UsageMeters &meters)
{
    const Onyx::DefaultResources &res = Onyx::Resources_GetDefault();
    u32 vertices = 0;
    u32 indices = 0;
    const auto pool = [&](const TKit::Span<const Onyx::Resource> meshes) {
        u32 v = 0;
        u32 i = 0;
        for (const Onyx::Resource mesh : meshes)
            addPoolFill(v, i, Onyx::Mesh_GetRange<D>(mesh));
        vertices = Math::Max(vertices, v);
        indices = Math::Max(indices, i);
    };
    if constexpr (D == D2)
    {
        const Onyx::SpecificDefaultResources<D2> &res2 = res.Resources2;
        const Onyx::Resource statics[] = {res2.Triangle, res2.Quad};
        const Onyx::Resource parametrics[] = {res2.Stadium, res2.RoundedRect};
        pool(TKit::Span<const Onyx::Resource>(statics));
        pool(TKit::Span<const Onyx::Resource>(parametrics));
        if (res.Font != Onyx::NullHandle)
            addPoolFill(vertices, indices, Onyx::Font_GetRange(res.Font));
    }
    else
    {
        const Onyx::SpecificDefaultResources<D3> &res3 = res.Resources3;
        const Onyx::Resource statics[] = {res3.Triangle, res3.Quad, res3.Box, res3.Sphere, res3.Cylinder};
        const Onyx::Resource parametrics[] = {res3.Stadium, res3.RoundedRect, res3.Capsule, res3.RoundedBox,
                                              res3.Torus};
        pool(TKit::Span<const Onyx::Resource>(statics));
        pool(TKit::Span<const Onyx::Resource>(parametrics));
    }
    meters.Now[Usage_PoolVertices] = vertices;
    meters.Now[Usage_PoolIndices] = indices;
}

template <Onyx::Dimension D>
static void measureUsage(UsageMeters &meters, const TKit::Span<const Onyx::RenderContext<D> *const> contexts)
{
    UsageTally<D> tally{};
    for (const Onyx::RenderContext<D> *ctx : contexts)
        if (ctx)
            tallyContext(tally, *ctx);

    TKit::FixedArray<u32, Usage_Count> &now = meters.Now;
    for (u32 geo = 0; geo < Onyx::Geometry_Count; ++geo)
        now[Usage_Circles + geo] = tally.Instances[geo];
    now[Usage_Lights] = 0;
    for (u32 type = 0; type < Onyx::LightTypeCount<D>; ++type)
        now[Usage_Lights] = Math::Max(now[Usage_Lights], tally.Lights[type]);
    if constexpr (D == D2)
    {
        now[Usage_ShadowMaps] = tally.ShadowMaps[0];
        // the 2D shadow pass renders one occlusion map for every shadow map it fills
        now[Usage_OcclusionMaps] = tally.ShadowMaps[0];
    }
    else
        for (u32 type = 0; type < Onyx::LightTypeCount<D3>; ++type)
            now[Usage_PointShadowMaps + type] = tally.ShadowMaps[type];
    now[Usage_DynamicVertices] = tally.DynamicVertices;
    now[Usage_DynamicIndices] = tally.DynamicIndices;
    u32 circleCommands = 0;
    u32 meshCommands = 0;
    for (u32 view = 0; view < ONYX_MAX_VIEWS; ++view)
    {
        circleCommands += tally.CircleCommands[view] + tally.ShadowRenders[view] * tally.ShadedCircles[view];
        meshCommands += tally.MeshCommands[view] + tally.ShadowRenders[view] * tally.ShadedMeshes[view];
    }
    now[Usage_DrawCommands] = Math::Max(circleCommands, meshCommands);
    measurePools<D>(meters);
    now[Usage_DynamicMeshes] = Onyx::DynamicMesh_GetCount<D>();

    const Onyx::SpecificSpecs<D> &specs = Onyx::GetSpecs().GetSpecific<D>();
    TKit::FixedArray<u32, Usage_Count> &limit = meters.Limit;
    for (u32 geo = 0; geo < Onyx::Geometry_Count; ++geo)
        limit[Usage_Circles + geo] = specs.MaxInstances;
    limit[Usage_Lights] = specs.MaxLightInstances;
    for (bool &applies : meters.Applies)
        applies = true;
    if constexpr (D == D2)
    {
        limit[Usage_ShadowMaps] = specs.MaxShadowMaps;
        limit[Usage_OcclusionMaps] = specs.MaxOcclusionMaps;
        for (u32 type = 0; type < Onyx::LightTypeCount<D3>; ++type)
            meters.Applies[Usage_PointShadowMaps + type] = false;
    }
    else
    {
        for (u32 type = 0; type < Onyx::LightTypeCount<D3>; ++type)
            limit[Usage_PointShadowMaps + type] = specs.MaxShadowMaps[type];
        meters.Applies[Usage_ShadowMaps] = false;
        meters.Applies[Usage_OcclusionMaps] = false;
    }
    limit[Usage_DynamicVertices] = specs.MaxDynamicVertices;
    limit[Usage_DynamicIndices] = specs.MaxDynamicIndices;
    limit[Usage_DrawCommands] = specs.MaxDrawCommands;
    limit[Usage_PoolVertices] = specs.MaxVerticesPerResourcePool;
    limit[Usage_PoolIndices] = specs.MaxIndicesPerResourcePool;
    limit[Usage_DynamicMeshes] = specs.MaxDynamicMeshes;

    for (u32 i = 0; i < Usage_Count; ++i)
        meters.Peak[i] = Math::Max(meters.Peak[i], now[i]);
}

// every context the renderer transfers. the overlay's floating windows, when promoted to windows of their own, have
// contexts of their own that are not reachable from here
static void measureUsage(App &app)
{
    const Onyx::NativeWindow *native = app.Ui->GetMainNativeWindow();
    const Onyx::RenderContext<D2> *contexts2[] = {app.PixelCtx,
                                                  app.GalleryCtx,
                                                  app.LabelCtx,
                                                  app.TexturesCtx,
                                                  app.RtCtx,
                                                  app.LabCtx,
                                                  native ? native->Context : nullptr};
    const Onyx::RenderContext<D3> *contexts3[] = {app.Ctx3};
    measureUsage<D2>(app.Usage[0], TKit::Span<const Onyx::RenderContext<D2> *const>(contexts2));
    measureUsage<D3>(app.Usage[1], TKit::Span<const Onyx::RenderContext<D3> *const>(contexts3));
}

// =====================================================================================================================
//  8. SCENE DRAWING
// =====================================================================================================================

// fine pattern in logical pixels. "align" puts integer coordinates on pixel centers, so 1 px features land exactly on
// one pixel instead of straddling two
static void drawPixelPattern(Onyx::RenderContext<D2> *ctx, const f32 halfExtent, const f32v2 &align)
{
    ctx->FillColor(Theme::Grid);
    for (f32 x = -halfExtent; x <= halfExtent; x += 8.f)
        ctx->Line(f32v2{x, -halfExtent} + align, f32v2{x, halfExtent} + align, 1.f);
    for (f32 y = -halfExtent; y <= halfExtent; y += 8.f)
        ctx->Line(f32v2{-halfExtent, y} + align, f32v2{halfExtent, y} + align, 1.f);

    ctx->FillColor(Onyx::Color_Coral);
    for (u32 i = 0; i < 16; ++i)
        for (u32 j = 0; j < 16; ++j)
            if ((i + j) % 2 == 0)
            {
                ctx->Push();
                ctx->Translate(f32v2{f32(i), f32(j)} - 8.f + align);
                ctx->Quad();
                ctx->Pop();
            }
}

// offset from integer world coordinates to pixel centers, for a viewport camera
static f32v2 pixelAlignment(const Onyx::RenderView<D2> *rv)
{
    const f32v2 corner = rv->ScreenToWorld(rv->GetViewport().Position);
    return corner - f32v2{std::floor(corner[0]), std::floor(corner[1])} + 0.5f;
}

// every pane's frame, in its 2D frame view: the title, brackets just inside the four corners (a missing one means the
// viewport or scissor is wrong) and a ring at the mouse. the visual review points at the view its current check
// concerns with longer, thicker brackets, which pulse for a moment when it moves on to another view
static void drawFrame(const App &app, const PaneId id)
{
    const Pane &pane = app.Panes[id];
    const PaneInfo &info = PaneInfos[id];
    Onyx::RenderContext<D2> *ctx = pane.Context;
    const Onyx::RenderView<D2> *rv = pane.Frame[app.Mode];
    const Onyx::Viewport &vp = rv->GetViewport();
    const f32v2 center = vp.Position + 0.5f * vp.Size;
    const f32 pixel = Math::Norm(rv->ScreenToWorld(center + f32v2{1.f, 0.f}) - rv->ScreenToWorld(center));

    // counter rotated with the camera, so it always reads upright
    ctx->Push();
    ctx->FillColor(info.Tint);
    ctx->Align(Onyx::Alignment_Center);
    ctx->Scale(15.f * pixel);
    ctx->Rotate(rv->GetCamera()->View.Rotation);
    ctx->Translate(rv->ScreenToWorld(vp.Position + f32v2{0.5f * vp.Size[0], 26.f}));
    ctx->Text(info.Title);
    ctx->Pop();

    const bool focus = app.FocusPane == id;
    const f32 arm = focus ? 32.f : 14.f;
    // a pulse that never stops reads as flicker: it only draws the eye when the review points somewhere new
    const f32 since = app.Time - app.FocusSince;
    const f32 wave = since < 1.2f ? Math::Sine(5.f * since) : 0.f;
    const f32 width = (focus ? 4.f + 2.f * wave * wave : 2.f) * pixel;
    const f32 inset = 3.f;
    const f32v2 corners[4] = {vp.Position + inset, vp.Position + f32v2{vp.Size[0] - inset, inset},
                              vp.Position + f32v2{inset, vp.Size[1] - inset}, vp.Position + vp.Size - inset};
    ctx->Push();
    ctx->FillColor(info.Tint);
    for (const f32v2 &c : corners)
    {
        const f32v2 towardsX = f32v2{c[0] < center[0] ? arm : -arm, 0.f};
        const f32v2 towardsY = f32v2{0.f, c[1] < center[1] ? arm : -arm};
        const f32v2 w = rv->ScreenToWorld(c);
        ctx->Line(w, rv->ScreenToWorld(c + towardsX), width);
        ctx->Line(w, rv->ScreenToWorld(c + towardsY), width);
    }
    ctx->Pop();

    if (!pane.MouseInside)
        return;
    ctx->Push();
    ctx->FillColor(Theme::Cursor);
    ctx->Scale(20.f * pixel);
    ctx->Translate(rv->ScreenToWorld(pane.Mouse));
    ctx->Circle({.Hollowness = 0.75f});
    ctx->Pop();
}

static void drawPixelView(const App &app)
{
    Onyx::RenderContext<D2> *ctx = app.PixelCtx;
    ctx->Flush();
    const f32v2 align = pixelAlignment(app.Panes[Pane_Pixel].Frame[app.Mode]);

    drawPixelPattern(ctx, 96.f, align);

    // outlined circle: the outline must keep its proportion at every scale
    ctx->Push();
    ctx->RenderFlags(Onyx::RenderModeFlag_Flat | Onyx::RenderModeFlag_Outlined);
    ctx->FillColor(Onyx::Color_Sky);
    ctx->OutlineColor(Onyx::Color_Honey);
    ctx->OutlineWidth(0.1f);
    ctx->Scale(64.f);
    ctx->Translate(f32v2{-140.f, 0.f});
    ctx->Circle();
    ctx->Pop();

    // a ruler: ticks every 10 logical pixels, longer every 50
    ctx->Push();
    ctx->FillColor(Theme::Dim);
    for (u32 i = 0; i <= 100; i += 10)
    {
        const f32 x = 110.f + f32(i);
        const f32 h = i % 50 == 0 ? 12.f : 6.f;
        ctx->Line(f32v2{x, -40.f} + align, f32v2{x, -40.f + h} + align, 1.f);
    }
    ctx->Line(f32v2{110.f, -40.f} + align, f32v2{210.f, -40.f} + align, 1.f);
    ctx->Pop();

    drawFrame(app, Pane_Pixel);
}

// the lights live in a dark receiver band under the primitives, with one occluder per geometry kind. the 2D shadow pass
// draws every shaded draw whose material is an occluder, so each kind must cast
constexpr f32v2 BandCenter{0.f, -3.f};
constexpr f32v2 BandSize{9.8f, 3.8f};

static void drawGallery2(App &app)
{
    Onyx::RenderContext<D2> *ctx = app.GalleryCtx;
    const RenderSettings &rs = app.Render;
    const LightSettings &ls = app.Lights;
    LightRequests &req = app.Requested;
    const Pane &pane = app.Panes[Pane_Gallery2];
    const Onyx::RenderView<D2> *rv = pane.Frame[app.Mode];
    const f32 time = app.Time;
    ctx->Flush();

    // background grid, one line per world unit
    ctx->FillColor(Theme::Grid);
    for (i32 i = -15; i <= 15; ++i)
    {
        ctx->Line(f32v2{f32(i), -15.f}, f32v2{f32(i), 15.f}, 0.02f);
        ctx->Line(f32v2{-15.f, f32(i)}, f32v2{15.f, f32(i)}, 0.02f);
    }

    if (ls.Lights2)
    {
        // the light orbits the center occluder on request, follows the cursor, or wanders between the occluders
        const f32v2 previous = req.Point2;
        if (ls.Sweep)
            req.Point2 =
                BandCenter + f32v2{0.f, 0.4f} + 1.6f * f32v2{Math::Cosine(1.2f * time), Math::Sine(1.2f * time)};
        else if (pane.MouseInside)
            req.Point2 = rv->ScreenToWorld(pane.Mouse);
        else
            req.Point2 = BandCenter + f32v2{3.8f * Math::Cosine(0.6f * time), 1.3f * Math::Sine(1.2f * time)};

        // a cone points where the light moves (smoothed, so it does not jitter), or at the slider angle
        if (app.DeltaTime > 0.f)
            req.Velocity2 += 0.15f * ((req.Point2 - previous) / app.DeltaTime - req.Velocity2);
        if (!ls.ConeFollowsMotion)
            req.ConeAim = ls.ConeAngle;
        else if (Math::Norm(req.Velocity2) > 0.3f)
            req.ConeAim = Math::AntiTangent(req.Velocity2[1], req.Velocity2[0]);

        // emitted under a non identity transform on purpose: lights take world space positions and angles, and the
        // light checks catch any transform leaking into them
        ctx->Push();
        ctx->Scale(2.f);
        ctx->Translate(f32v2{5.f, -7.f});
        ctx->PointLight({.Position = req.Point2,
                         .Tint = Onyx::Color_Honey,
                         .LightSize = ls.LightSize2,
                         .LightRadius = 4.f,
                         .ShadowRadius = 8.f,
                         .Angle = ls.Cone ? req.ConeAim : 0.f,
                         .Decay = ls.Cone ? ls.ConeDecay : 0.f,
                         .Extent = ls.Cone ? ls.ConeExtent : 1.f,
                         .Intensity = 1.2f,
                         .Flags = FilterFlags[ls.Filter2]});
        // a band of light along its direction, wide enough to cover the gallery. its shadows run from the occluders in
        // the direction it travels
        if (ls.Directional2)
            ctx->DirectionalLight({.Tint = Onyx::Color_Sky,
                                   .LightExtent = 12.f,
                                   .ShadowExtent = 12.f,
                                   .Angle = ls.DirectionalAngle,
                                   .AngleSize = ls.SunAngle2,
                                   .Intensity = 0.55f,
                                   .Flags = FilterFlags[ls.Filter2]});
        ctx->Pop();

        ctx->Push();
        ctx->RenderFlags(Onyx::RenderModeFlag_Shaded);
        ctx->Material(app.Lit2);
        ctx->FillColor(Onyx::Color{70u, 74u, 96u});
        ctx->Scale(BandSize);
        ctx->Translate(BandCenter);
        ctx->Quad();

        // occluders at different distances, all moving: static meshes (the spinning quad the sweep orbits, a triangle),
        // a circle, parametric meshes (stadium, rounded rect) and glyphs
        ctx->Material(app.Occluder2);
        ctx->FillColor(Onyx::Color_Rose);
        ctx->ResetTransform();
        ctx->Scale(0.6f);
        ctx->Rotate(time);
        ctx->Translate(BandCenter + f32v2{0.f, 0.4f});
        ctx->Quad();

        ctx->ResetTransform();
        ctx->Scale(0.7f);
        ctx->Rotate(-0.8f * time);
        ctx->Translate(BandCenter + f32v2{-3.3f, 0.8f});
        ctx->Triangle();

        ctx->ResetTransform();
        ctx->Scale(0.6f);
        ctx->Translate(BandCenter + f32v2{-1.2f + 0.4f * Math::Sine(0.9f * time), -0.6f});
        ctx->Circle();

        ctx->ResetTransform();
        ctx->Scale(0.55f);
        ctx->Rotate(0.6f * time);
        ctx->Translate(BandCenter + f32v2{1.7f, -0.9f});
        ctx->Stadium();

        ctx->ResetTransform();
        ctx->Scale(0.65f);
        ctx->Rotate(-0.4f * time);
        ctx->Translate(BandCenter + f32v2{3.3f, 0.7f});
        ctx->RoundedRect();

        ctx->ResetTransform();
        ctx->Align(Onyx::Alignment_Center);
        ctx->Scale(0.6f);
        ctx->Translate(BandCenter + f32v2{-3.f, -1.2f + 0.15f * Math::Sine(1.3f * time)});
        ctx->Text("ONYX");
        ctx->Pop();

        // flat markers, so they never reach the shadow maps: a ring on the point light, and an arrow showing where the
        // directional light travels
        ctx->Push();
        ctx->FillColor(Onyx::Color_Honey);
        ctx->Scale(0.3f);
        ctx->Translate(req.Point2);
        ctx->Circle({.Hollowness = 0.6f});
        ctx->Pop();
        if (ls.Directional2)
        {
            const f32 a = ls.DirectionalAngle;
            const f32v2 dir{Math::Cosine(a), Math::Sine(a)};
            const f32v2 left{Math::Cosine(a + 2.6f), Math::Sine(a + 2.6f)};
            const f32v2 right{Math::Cosine(a - 2.6f), Math::Sine(a - 2.6f)};
            const f32v2 tip = BandCenter + f32v2{4.1f, 1.45f} + 0.4f * dir;
            ctx->Push();
            ctx->FillColor(Onyx::Color_Sky);
            ctx->RoundedLine(tip - 0.8f * dir, tip, 0.06f);
            ctx->RoundedLine(tip, tip + 0.3f * left, 0.06f);
            ctx->RoundedLine(tip, tip + 0.3f * right, 0.06f);
            ctx->Pop();
        }
    }

    // top row: every 2D primitive, with the selected render mode
    Onyx::RenderModeFlags flags = Onyx::RenderModeFlag_Flat;
    if (rs.Outlines)
        flags |= Onyx::RenderModeFlag_Outlined;
    ctx->RenderFlags(flags);
    ctx->OutlineColor(Theme::Text);
    ctx->OutlineWidth(rs.OutlineWidth);

    for (u32 i = 0; i < 5; ++i)
    {
        ctx->Push();
        ctx->FillColor(Theme::Shapes[i]);
        ctx->Scale(1.2f);
        if (rs.Rotate && i != 2)
            ctx->Rotate(0.5f * time + f32(i));
        ctx->Translate(f32v2{-3.6f + 1.8f * f32(i), 3.f});
        switch (i)
        {
        case 0:
            ctx->Triangle();
            break;
        case 1:
            ctx->Quad();
            break;
        case 2:
            if (rs.Circle.InnerFade > 0.f || rs.Circle.OuterFade > 0.f)
                ctx->Blend();
            ctx->Circle(rs.Circle);
            break;
        case 3:
            ctx->Stadium();
            break;
        default:
            ctx->RoundedRect();
            break;
        }
        ctx->Pop();
    }

    // middle row: three overlapping translucent squares, between two lines
    ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
    for (u32 i = 0; i < 3; ++i)
    {
        ctx->Push();
        ctx->FillColor(Theme::Shapes[i + 3]);
        ctx->Alpha(rs.Alpha);
        ctx->Scale(1.3f);
        ctx->Translate(f32v2{-0.6f + 0.6f * f32(i), 0.6f + 0.3f * f32(i % 2)});
        ctx->Quad();
        ctx->Pop();
    }
    ctx->Push();
    ctx->FillColor(Onyx::Color_Mint);
    ctx->RoundedLine(f32v2{-4.4f, 0.3f}, f32v2{-2.2f, 1.1f}, 0.15f);
    ctx->FillColor(Onyx::Color_Peach);
    ctx->Line(f32v2{2.2f, 1.1f}, f32v2{4.4f, 0.3f}, 0.15f);
    ctx->Pop();

#ifdef ONYX_ENABLE_AUDIO
    // the 2D spatial orbit: the listener and the source circling it
    const AudioState &au = app.Audio;
    if (au.Available && au.Orbit == Orbit_2D)
    {
        ctx->Push();
        ctx->FillColor(Theme::Accent);
        ctx->Line(au.Listener2, au.Source2, 0.03f);
        ctx->Scale(0.5f);
        ctx->Translate(au.Listener2);
        ctx->Circle({.Hollowness = 0.7f});
        ctx->ResetTransform();
        ctx->Scale(0.35f);
        ctx->Translate(au.Source2);
        ctx->Circle();
        ctx->Pop();
    }
#endif

    drawFrame(app, Pane_Gallery2);
}

// the spot's cone: lines from the light to where its edge meets the floor, and the outline of the lit area there. drawn
// flat, like the light markers, so it never reaches the shadow maps
static void drawSpotCone(Onyx::RenderContext<D3> *ctx, const f32v3 &position, const f32v3 &direction, const f32 fov,
                         const f32 floorY)
{
    constexpr u32 segments = 48;
    // the helper vector is picked here, not by the one argument CreateOrthogonalBase: that one breaks straight down
    // (see the light checks), and the outline must stay right when the spot points there
    const f32v3 helper = Math::Absolute(direction[1]) < 0.99f ? f32v3{0.f, 1.f, 0.f} : f32v3{1.f, 0.f, 0.f};
    const f32m2x3 base = Onyx::Transform<D3>::CreateOrthogonalBase(direction, helper);
    const f32 half = 0.5f * fov;

    // where each edge ray meets the floor, or a point far along it when it never does
    TKit::FixedArray<f32v3, segments> edge{};
    TKit::FixedArray<bool, segments> onFloor{};
    for (u32 i = 0; i < segments; ++i)
    {
        const f32 a = 2.f * Math::Pi<f32>() * f32(i) / f32(segments);
        const f32v3 ray =
            Math::Cosine(half) * direction + Math::Sine(half) * (Math::Cosine(a) * base[0] + Math::Sine(a) * base[1]);
        const f32 t = ray[1] < -1e-3f ? (floorY - position[1]) / ray[1] : 30.f;
        onFloor[i] = t < 30.f;
        edge[i] = position + Math::Min(t, 30.f) * ray;
    }

    ctx->Push();
    ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
    ctx->FillColor(Onyx::Color_Sky);
    ctx->Line(position, position + 0.8f * direction, 0.04f);
    const f32v3 lift{0.f, 0.02f, 0.f};
    for (u32 i = 0; i < segments; ++i)
    {
        if (i % (segments / 8) == 0)
            ctx->Line(position, edge[i], 0.015f);
        const u32 next = (i + 1) % segments;
        if (onFloor[i] && onFloor[next])
            ctx->Line(edge[i] + lift, edge[next] + lift, 0.03f);
    }
    ctx->Scale(0.22f);
    ctx->Translate(position);
    ctx->Sphere();
    ctx->Pop();
}

static void drawGallery3(App &app)
{
    Onyx::RenderContext<D3> *ctx = app.Ctx3;
    const RenderSettings &rs = app.Render;
    const LightSettings &ls = app.Lights;
    LightRequests &req = app.Requested;
    constexpr f32 floorY = -1.6f;
    ctx->Flush();

    Onyx::RenderModeFlags flags = Onyx::RenderModeFlag_Shaded;
    if (rs.Outlines)
        flags |= Onyx::RenderModeFlag_Outlined;
    ctx->RenderFlags(flags);
    ctx->OutlineColor(Theme::Text);
    ctx->OutlineWidth(rs.OutlineWidth);
    ctx->Material(app.Mat3);

    // spot only leaves the spot alone, so its light and shadows are unmistakable
    const bool others = ls.Lights3 && !ls.SpotOnly;
    if (others)
        ctx->DirectionalLight({.Direction = ls.SunDown ? f32v3{0.f, -1.f, 0.f} : f32v3{-0.6f, -1.f, -0.4f},
                               .Cascades = {.View = app.View3[app.Mode]},
                               .AngleSize = ls.SunAngle3,
                               .Intensity = 0.7f,
                               .Flags = FilterFlags[ls.Filter3]});
    if (others && ls.Point3)
    {
        req.Point3 = f32v3{3.5f * Math::Cosine(app.Time), 2.5f, 3.5f * Math::Sine(app.Time)};
        // emitted under a non identity transform on purpose (see the light checks). ShadowRadius is the far plane of
        // the cube shadow map and shapes here can be about 10 units away from the light. the near plane is 5% of it
        // and shadows fade out over its last 10%, so it must cover the scene without overdoing it
        ctx->Push();
        ctx->Translate(f32v3{1.f, 2.f, 3.f});
        ctx->PointLight({.Position = req.Point3,
                         .Tint = Onyx::Color_Honey,
                         .LightSize = ls.PointSize3,
                         .LightRadius = 6.f,
                         .ShadowRadius = 14.f,
                         .Intensity = 1.6f,
                         .Flags = FilterFlags[ls.Filter3]});
        ctx->Pop();

        // a flat marker: a shaded sphere around the light would block its own shadows
        ctx->Push();
        ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
        ctx->FillColor(Onyx::Color_Buttercup);
        ctx->Scale(0.25f);
        ctx->Translate(req.Point3);
        ctx->Sphere();
        ctx->Pop();
    }
    if (ls.Lights3 && ls.Spot3)
    {
        // tilted away from straight down, towards SpotTurn. swinging adds a slow sway to the tilt. ShadowRange is the
        // far plane of its shadow map (the near plane is a fraction of it) and shadows fade over its last 10%: 20 units
        // keep the floor well inside it, even tilted
        const f32 tilt = ls.SpotTilt + (ls.SpotSwing ? Math::Radians(12.f) * Math::Sine(0.7f * app.Time) : 0.f);
        req.SpotDirection = Math::Normalize(f32v3{Math::Sine(tilt) * Math::Cosine(ls.SpotTurn), -Math::Cosine(tilt),
                                                  Math::Sine(tilt) * Math::Sine(ls.SpotTurn)});
        ctx->Push();
        ctx->Translate(f32v3{-2.f, 1.f, 0.5f});
        ctx->SpotLight({.Position = req.SpotPosition,
                        .Direction = req.SpotDirection,
                        .Tint = Onyx::Color_Sky,
                        .LightSize = ls.SpotSize3,
                        .FieldOfView = ls.SpotFov,
                        .LightRange = ls.SpotRange,
                        .ShadowRange = 20.f,
                        .Intensity = 1.8f,
                        .Flags = FilterFlags[ls.Filter3]});
        ctx->Pop();
        drawSpotCone(ctx, req.SpotPosition, req.SpotDirection, ls.SpotFov, floorY);
    }

    // floor
    ctx->Push();
    ctx->RemoveRenderFlags(Onyx::RenderModeFlag_Outlined);
    ctx->FillColor(Onyx::Color{90u, 92u, 110u});
    ctx->ScaleX(24.f);
    ctx->ScaleY(24.f);
    ctx->RotateX(-0.5f * Math::Pi<f32>());
    ctx->TranslateY(floorY);
    ctx->Quad();
    ctx->Pop();

    // one of each 3D primitive
    for (u32 i = 0; i < 6; ++i)
    {
        ctx->Push();
        ctx->FillColor(Theme::Shapes[i]);
        ctx->Scale(1.2f);
        if (rs.Rotate)
            ctx->RotateY(0.6f * app.Time + f32(i));
        ctx->Translate(f32v3{-6.5f + 2.6f * f32(i), 0.f, 0.f});
        switch (i)
        {
        case 0:
            ctx->Box();
            break;
        case 1:
            ctx->Sphere();
            break;
        case 2:
            ctx->Cylinder();
            break;
        case 3:
            ctx->Capsule();
            break;
        case 4:
            ctx->RoundedBox();
            break;
        default:
            ctx->Torus();
            break;
        }
        ctx->Pop();
    }

    // a box right under the spot: lit by it, and casting its shadow onto the floor
    ctx->Push();
    ctx->FillColor(Onyx::Color_Pearl);
    ctx->Scale(0.9f);
    ctx->RotateY(0.4f * app.Time);
    ctx->Translate(f32v3{req.SpotPosition[0], 0.3f, req.SpotPosition[2]});
    ctx->Box();
    ctx->Pop();

    // translucent shapes in front, at staggered depths: they overlap each other and the opaque row behind them
    for (u32 i = 0; i < 3; ++i)
    {
        ctx->Push();
        ctx->FillColor(Theme::Shapes[2 * i]);
        ctx->Alpha(rs.Alpha3);
        ctx->Scale(1.1f);
        ctx->RotateY(0.3f * app.Time + f32(i));
        ctx->Translate(f32v3{2.6f + 0.8f * f32(i), 0.2f, 4.f - 0.8f * f32(i)});
        if (i == 1)
            ctx->Sphere();
        else
            ctx->Box();
        ctx->Pop();
    }

#ifdef ONYX_ENABLE_AUDIO
    // the 3D spatial orbit circles the camera, which is the listener: it crosses the view when it passes in front
    const AudioState &au = app.Audio;
    if (au.Available && au.Orbit == Orbit_3D)
    {
        ctx->Push();
        ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
        ctx->FillColor(Theme::Accent);
        ctx->Scale(0.2f);
        ctx->Translate(au.Source3);
        ctx->Sphere();
        ctx->Pop();
    }
#endif

    // the title, brackets and cursor of the 3D gallery go to the transparent 2D view stacked on top of it
    app.LabelCtx->Flush();
    drawFrame(app, Pane_Gallery3);
}

static void drawTextures(const App &app)
{
    // render texture contents: the same pixel pattern as the pixel view, in each texture's own logical pixels
    Onyx::RenderContext<D2> *rt = app.RtCtx;
    rt->Flush();
    drawPixelPattern(rt, 64.f, f32v2{0.5f});
    rt->Push();
    rt->FillColor(Theme::Text);
    rt->Align(Onyx::Alignment_Center);
    rt->Scale(14.f);
    rt->Translate(f32v2{0.f, 70.f});
    rt->Text("Render texture");
    rt->Pop();

    Onyx::RenderContext<D2> *ctx = app.TexturesCtx;
    ctx->Flush();
    const f32v2 align = pixelAlignment(app.Panes[Pane_Textures].Frame[app.Mode]);
    const f32 size = f32(RenderTextureSize[0]);

    // drawn at their logical size, so the scaled one maps one texel to one physical pixel
    const Onyx::RenderTexture *textures[2] = {app.RtScaled, app.RtFixed};
    const char *labels[2] = {"Follows window scale", "Fixed at scale 1"};
    for (u32 i = 0; i < 2; ++i)
    {
        const f32 x = i == 0 ? -100.f : 100.f;
        ctx->Push();
        ctx->Scale(size);
        ctx->Translate(f32v2{x, 0.f} + align - 0.5f);
        ctx->Image(*textures[i]);
        ctx->ResetTransform();
        ctx->FillColor(Theme::Dim);
        ctx->Align(Onyx::Alignment_Center);
        ctx->Scale(13.f);
        ctx->Translate(f32v2{x, -100.f});
        ctx->Text(labels[i]);
        ctx->Pop();
    }

    drawFrame(app, Pane_Textures);
}

// the lab: a world grid with labelled axes, shapes for each flag (outlined ones, a translucent square, thin lines and a
// circle for FXAA), brackets at the viewport corners, the scissor outline, a probe point and the cursor ring
static void drawLab(const App &app)
{
    Onyx::RenderContext<D2> *ctx = app.LabCtx;
    ctx->Flush();
    const LabSettings &lab = app.Lab;
    const Onyx::RenderView<D2> *rv = app.LabView;
    if (rv->GetFlags() & Onyx::RenderViewFlag_Hidden)
        return;

    const Onyx::Viewport &vp = rv->GetViewport();
    const Onyx::Scissor &sc = rv->GetScissor();
    const f32v2 center = vp.Position + 0.5f * vp.Size;
    const f32 pixel = Math::Norm(rv->ScreenToWorld(center + f32v2{1.f, 0.f}) - rv->ScreenToWorld(center));
    const f32 step = lab.Orthographic ? 1.f : 50.f; // 1 world unit, or 50 logical pixels in viewport mode

    // the world rectangle the viewport covers. the camera may be rotated, so all four corners count
    const f32v2 corners[4] = {vp.Position, vp.Position + f32v2{vp.Size[0], 0.f}, vp.Position + f32v2{0.f, vp.Size[1]},
                              vp.Position + vp.Size};
    f32v2 lo = rv->ScreenToWorld(corners[0]);
    f32v2 hi = lo;
    for (const f32v2 &c : corners)
    {
        lo = Math::Min(lo, rv->ScreenToWorld(c));
        hi = Math::Max(hi, rv->ScreenToWorld(c));
    }
    ctx->FillColor(Theme::Grid);
    for (u32 i = 0; i < 200; ++i)
    {
        const f32 x = (std::floor(lo[0] / step) + f32(i)) * step;
        if (x > hi[0])
            break;
        ctx->Line(f32v2{x, lo[1]}, f32v2{x, hi[1]}, pixel);
    }
    for (u32 i = 0; i < 200; ++i)
    {
        const f32 y = (std::floor(lo[1] / step) + f32(i)) * step;
        if (y > hi[1])
            break;
        ctx->Line(f32v2{lo[0], y}, f32v2{hi[0], y}, pixel);
    }
    ctx->FillColor(Onyx::Color_Coral);
    ctx->Line(f32v2{lo[0], 0.f}, f32v2{hi[0], 0.f}, 2.f * pixel);
    ctx->FillColor(Onyx::Color_Mint);
    ctx->Line(f32v2{0.f, lo[1]}, f32v2{0.f, hi[1]}, 2.f * pixel);

    // labels at the origin and two steps along each axis, kept upright
    const f32v2 labelled[3] = {f32v2{0.f}, f32v2{2.f * step, 0.f}, f32v2{0.f, 2.f * step}};
    for (const f32v2 &at : labelled)
    {
        ctx->Push();
        ctx->FillColor(Theme::Text);
        ctx->Scale(12.f * pixel);
        ctx->Rotate(lab.Rotation);
        ctx->Translate(at + 8.f * pixel);
        ctx->Text(formatLine("({:g}, {:g})", at[0], at[1]).View());
        ctx->Pop();
    }

    // outlines need RenderViewFlag_Outlines, the translucent square RenderViewFlag_Transparency. the thin lines and the
    // circle are where FXAA shows most
    ctx->Push();
    ctx->RenderFlags(Onyx::RenderModeFlag_Flat | Onyx::RenderModeFlag_Outlined);
    ctx->OutlineColor(Theme::Text);
    ctx->OutlineWidth(0.08f);
    ctx->FillColor(Onyx::Color_Sky);
    ctx->Scale(1.4f * step);
    ctx->Translate(f32v2{-2.f * step, 1.2f * step});
    ctx->Circle();
    ctx->ResetTransform();
    ctx->FillColor(Onyx::Color_Honey);
    ctx->Scale(1.2f * step);
    ctx->Rotate(0.4f * app.Time);
    ctx->Translate(f32v2{2.f * step, 1.2f * step});
    ctx->Quad();
    ctx->Pop();

    ctx->Push();
    ctx->FillColor(Onyx::Color_Lavender);
    ctx->Alpha(0.5f);
    ctx->Scale(1.6f * step);
    ctx->Translate(f32v2{0.f, -1.f * step});
    ctx->Quad();
    ctx->Pop();

    ctx->Push();
    ctx->FillColor(Theme::Text);
    const f32v2 fan = f32v2{-2.f * step, -1.2f * step};
    for (u32 i = 0; i < 9; ++i)
    {
        const f32 a = 0.05f + 0.17f * f32(i);
        ctx->Line(fan, fan + 1.1f * step * f32v2{Math::Cosine(a), Math::Sine(a)}, pixel);
    }
    ctx->Pop();

    // the probe the panel converts to screen coordinates
    const f32v2 probe{step, step};
    ctx->Push();
    ctx->FillColor(Theme::Cursor);
    ctx->Line(probe - f32v2{6.f * pixel, 0.f}, probe + f32v2{6.f * pixel, 0.f}, 2.f * pixel);
    ctx->Line(probe - f32v2{0.f, 6.f * pixel}, probe + f32v2{0.f, 6.f * pixel}, 2.f * pixel);
    ctx->Pop();

    // brackets in the viewport's corners: only the ones the scissor lets through show. the scissor's own outline is
    // drawn a pixel inside it, so it is visible exactly when the scissor is right
    ctx->Push();
    ctx->FillColor(Theme::Accent);
    for (const f32v2 &c : corners)
    {
        const f32v2 inner = c + 3.f * f32v2{c[0] < center[0] ? 1.f : -1.f, c[1] < center[1] ? 1.f : -1.f};
        const f32v2 armX = f32v2{c[0] < center[0] ? 18.f : -18.f, 0.f};
        const f32v2 armY = f32v2{0.f, c[1] < center[1] ? 18.f : -18.f};
        ctx->Line(rv->ScreenToWorld(inner), rv->ScreenToWorld(inner + armX), 2.f * pixel);
        ctx->Line(rv->ScreenToWorld(inner), rv->ScreenToWorld(inner + armY), 2.f * pixel);
    }
    const f32v2 s0 = vp.Position + sc.Position + 1.5f;
    const f32v2 s1 = vp.Position + sc.Position + sc.Size - 1.5f;
    const f32v2 outline[4] = {s0, f32v2{s1[0], s0[1]}, s1, f32v2{s0[0], s1[1]}};
    for (u32 i = 0; i < 4; ++i)
        ctx->Line(rv->ScreenToWorld(outline[i]), rv->ScreenToWorld(outline[(i + 1) % 4]), pixel);
    ctx->Pop();

    if (!app.LabMouseInside)
        return;
    ctx->Push();
    ctx->FillColor(Theme::Cursor);
    ctx->Scale(20.f * pixel);
    ctx->Translate(rv->ScreenToWorld(app.Window->GetMousePosition()));
    ctx->Circle({.Hollowness = 0.75f});
    ctx->Pop();
}

static void drawScene(App &app)
{
    // views cache their matrices when they render. cameras have moved since, and docked textures may have been resized,
    // so refresh them first, or every screen to world conversion below (and in the checks) would lag one frame behind
    for (Pane &pane : app.Panes)
        pane.Frame[app.Mode]->CacheMatrices();
    app.View3[app.Mode]->CacheMatrices();
    app.LabView->CacheMatrices();

    drawPixelView(app);
    drawGallery2(app);
    drawGallery3(app);
    drawTextures(app);
    drawLab(app);
}

// =====================================================================================================================
//  9. AUDIO
// =====================================================================================================================

#ifdef ONYX_ENABLE_AUDIO
constexpr f32 ArpeggioRatios[4] = {1.f, 1.25992f, 1.49831f, 2.f}; // root, major third, fifth, octave
constexpr f32 ArpeggioNote = 0.18f;                               // seconds per note

// how long the instrument plays each wave, in seconds
constexpr f32 WaveLengths[Wave_Count] = {1.f, 1.f, 0.4f, 4.f * ArpeggioNote, 0.f};
constexpr const char *WaveNames = "Sine#Square#Noise burst#Arpeggio#Beeps";

// fills interleaved frames and returns how many it wrote. returning 0 ends the sound. runs on the audio thread: it
// only reads and writes the synth's atomics
static u32 synthesize(Synth &synth, f32 *out, const u32 frameCount, const u32 channels)
{
    const u32 length = synth.Length.load(std::memory_order_relaxed);
    u32 cursor = synth.Cursor.load(std::memory_order_relaxed);
    const u32 frames = length == 0 ? frameCount : Math::Min(frameCount, length - Math::Min(cursor, length));

    const u32 shape = synth.Shape.load(std::memory_order_relaxed);
    const f32 frequency = synth.Frequency.load(std::memory_order_relaxed);
    const f32 rate = f32(synth.SampleRate);
    const f32 fade = 0.005f * rate; // 5 ms ramps at both ends, so starting and ending do not click
    f32 phase = synth.Phase.load(std::memory_order_relaxed);
    u32 noise = synth.Noise.load(std::memory_order_relaxed);
    u32 head = synth.ScopeHead.load(std::memory_order_relaxed);

    for (u32 i = 0; i < frames; ++i, ++cursor)
    {
        const f32 time = f32(cursor) / rate;
        const f32 sine = Math::Sine(2.f * Math::Pi<f32>() * phase);
        f32 step = frequency;
        f32 sample = sine;
        switch (shape)
        {
        case Wave_Square:
            sample = phase < 0.5f ? 0.4f : -0.4f;
            break;
        case Wave_Noise:
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            sample = (f32(noise) / 4294967295.f * 2.f - 1.f) * std::exp(-6.f * time);
            break;
        case Wave_Arpeggio:
            step = frequency * ArpeggioRatios[u32(time / ArpeggioNote) % 4];
            break;
        case Wave_Beeps: {
            // 80 ms beeps every 300 ms, each with its own ramps. easier to place in space than a steady tone
            const f32 t = Math::Modulo(time, 0.3f);
            sample = t < 0.08f ? sine * Math::Min(1.f, Math::Min(t, 0.08f - t) / 0.005f) : 0.f;
            break;
        }
        default:
            break;
        }
        phase += step / rate;
        phase -= std::floor(phase);

        f32 envelope = Math::Min(1.f, f32(cursor) / fade);
        if (length != 0)
            envelope = Math::Min(envelope, f32(length - cursor) / fade);
        sample *= envelope;

        for (u32 c = 0; c < channels; ++c)
            out[i * channels + c] = sample;
        if (cursor % ScopeStride == 0)
            synth.Scope[head++ % ScopeSize].store(sample, std::memory_order_relaxed);
    }
    synth.Cursor.store(cursor, std::memory_order_relaxed);
    synth.Phase.store(phase, std::memory_order_relaxed);
    synth.Noise.store(noise, std::memory_order_relaxed);
    synth.ScopeHead.store(head, std::memory_order_release);
    return frames;
}

// custom sources need a seek callback to loop or to play again after ending: miniaudio does both by seeking back to
// frame 0. it may run on the main thread (Audio_Play after the end) or the audio thread (looping, deferred seeks)
static Onyx::Resource createSource(Synth &synth, const Onyx::AudioFlags flags)
{
    synth.SampleRate = Onyx::Audio_GetSampleRate();
    return Onyx::Audio_Create([&synth](f32 *out, const u32 frameCount,
                                       const u32 channels) { return synthesize(synth, out, frameCount, channels); },
                              {.SeekCallback =
                                   [&synth](const u64 frame) {
                                       synth.Cursor.store(u32(frame), std::memory_order_relaxed);
                                       synth.Phase.store(0.f, std::memory_order_relaxed);
                                       return true;
                                   },
                               .Flags = flags});
}

static void setupAudio(App &app)
{
    AudioState &au = app.Audio;
    au.Available = Onyx::Audio_IsAvailable();
    if (!au.Available)
        return;

    au.Instrument = createSource(au.InstrumentSynth, 0);
    au.Spatial = createSource(au.SpatialSynth, Onyx::AudioFlag_Spatialization | Onyx::AudioFlag_Loop);

    au.SpatialSynth.Shape.store(Wave_Beeps);
    au.SpatialSynth.Frequency.store(660.f);
    Onyx::Audio_SetMainThreadEndCallback(au.Instrument, [&au] { ++au.InstrumentEnds; });
}

// one column of the audio scope: a bar of height |value| * half, above the middle line when positive
static void scopeColumn(Onyx::Overlay *ui, const f32 value, const f32 height, const Onyx::Color &color)
{
    const f32 half = 0.5f * height;
    const f32 v = Math::Clamp(value, -1.f, 1.f);
    ui->BeginPanel(
        {.Direction = Onyx::LayoutDirection_TopToBottom, .Sizing = Onyx::LayoutSizing::Fixed(f32v2{2.f, height})});
    ui->Panel({.Sizing = Onyx::LayoutSizing::Fixed(f32v2{2.f, half * (1.f - Math::Max(v, 0.f))})});
    ui->Panel({.FillColor = color,
               .Sizing = Onyx::LayoutSizing::Fixed(f32v2{2.f, Math::Max(1.f, half * Math::Absolute(v))})});
    ui->EndPanel();
}

static void playInstrument(AudioState &au)
{
    au.InstrumentSynth.Length.store(u32(WaveLengths[au.Wave] * f32(au.InstrumentSynth.SampleRate)));
    Onyx::Audio_PlayFromBeginning(au.Instrument);
}

// the panel's settings are applied every frame
static void updateAudio(App &app)
{
    AudioState &au = app.Audio;
    if (!au.Available)
        return;

    au.InstrumentSynth.Shape.store(au.Wave, std::memory_order_relaxed);
    au.InstrumentSynth.Frequency.store(au.Frequency, std::memory_order_relaxed);
    Onyx::Audio_SetVolume(au.Instrument, au.Volume);
    Onyx::Audio_SetPitch(au.Instrument, au.Pitch);
    Onyx::Audio_SetPan(au.Instrument, au.Pan);
    Onyx::Audio_EnableLooping(au.Instrument, au.Loop);
    Onyx::Audio_SetVolume(au.Master);

    // the spatial source orbits the listener: the 2D gallery's center in 2D, the 3D camera in 3D. the listener looks
    // down -z in 2D, so x maps to left and right
    if (au.Orbit == Orbit_Off)
    {
        if (Onyx::Audio_IsPlaying(au.Spatial))
            Onyx::Audio_Stop(au.Spatial);
    }
    else
    {
        if (!Onyx::Audio_IsPlaying(au.Spatial))
            Onyx::Audio_Play(au.Spatial);
        const f32 a = 0.9f * app.Time;
        if (au.Orbit == Orbit_2D)
        {
            au.Listener2 = f32v2{0.f, 0.8f};
            au.Source2 = au.Listener2 + 3.f * f32v2{Math::Cosine(a), Math::Sine(a)};
            Onyx::Listener_SetPosition<D2>(0, au.Listener2);
            Onyx::Listener_SetDirection(0, f32v3{0.f, 0.f, -1.f});
            Onyx::Audio_SetPosition<D2>(au.Spatial, au.Source2);
        }
        else
        {
            au.Listener3 = app.Cam3.View.Translation;
            au.Source3 = au.Listener3 + 3.f * f32v3{Math::Cosine(a), 0.f, Math::Sine(a)};
            Onyx::Listener_SetPosition<D3>(0, au.Listener3);
            Onyx::Listener_SetDirection(0, Math::Normalize(app.Cam3.View.Rotation) * f32v3{0.f, 0.f, -1.f});
            Onyx::Audio_SetPosition<D3>(au.Spatial, au.Source3);
        }
    }
}
#endif

// =====================================================================================================================
//  10. OVERLAY PANELS
// =====================================================================================================================

constexpr const char *LegendPopup = "Legend##popup";

constexpr u32v2 SizePresets[3] = {{1280, 720}, {1600, 900}, {960, 540}};

constexpr const char *EventNames[] = {
    "KeyPressed",     "KeyReleased",        "KeyRepeat",           "MouseMoved",   "MousePressed",
    "MouseReleased",  "MouseEntered",       "MouseLeft",           "Scrolled",     "WindowMoved",
    "WindowResized",  "WindowFocused",      "WindowUnfocused",     "WindowClosed", "WindowMinimized",
    "WindowRestored", "FramebufferResized", "ContentScaleChanged", "CharInput",    "SwapchainRecreated",
};
static_assert(sizeof(EventNames) / sizeof(EventNames[0]) == Onyx::Event_Count,
              "EventNames must list every Onyx::EventType, in order");

static bool isApplicable(const App &app, const VisualCheck &check)
{
    switch (check.Needs)
    {
    case Requires_DirectLayout:
        return app.Mode == Mode_Direct;
    case Requires_DockedLayout:
        return app.Mode == Mode_Docked;
    case Requires_Audio:
#ifdef ONYX_ENABLE_AUDIO
        return app.Audio.Available;
#else
        return false;
#endif
    default:
        return true;
    }
}

// panels float in the direct layout, where they were born, and never dock: NoDocking also undocks a window that was
// docked. leaving the docked layout puts them back in place for two frames, since the first one only undocks them. in
// the docked layout the dock tree places them
static Onyx::OverlayWindowFlags placeWindow(const App &app, const f32v2 &position, const f32v2 &size)
{
    if (app.Mode == Mode_Docked)
        return 0;
    const Onyx::OverlayCondition cond =
        app.PlacePanels > 0 ? Onyx::OverlayCondition_Always : Onyx::OverlayCondition_OnBirth;
    app.Ui->SetNextWindowPosition(position, cond);
    app.Ui->SetNextWindowSize(size, cond);
    return Onyx::OverlayWindowFlag_NoDocking;
}

static void startPresentCycle(App &app)
{
    app.PresentCycleRestore = app.PresentMode;
    app.PresentCycleSteps = Graph::PresentMode_Count;
    app.PresentCycleAt = app.Time;
}

// -- visual review ---------------------------------------------------------------------------------------------------

struct GuideStep
{
    Category *Cat = nullptr;
    VisualCheck *Visual = nullptr; // the next visual check waiting for a verdict, by priority
    u32 Done = 0;
    u32 Total = 0;
};

static GuideStep findGuideStep(App &app)
{
    GuideStep step{};
    for (Category &cat : app.Categories)
        for (VisualCheck &check : cat.Visuals)
        {
            if (!isApplicable(app, check))
                continue;
            ++step.Total;
            if (check.Mark != Verdict_Unseen)
                ++step.Done;
            else if (!step.Visual || check.Priority < step.Visual->Priority)
            {
                step.Visual = &check;
                step.Cat = &cat;
            }
        }
    return step;
}

static Line describeWhere(const App &app, const Category &cat, const PaneId pane)
{
    if (pane == Pane_None)
        return formatLine("the {} panel", cat.Title);
    if (app.Mode == Mode_Direct)
        return formatLine("the {}, {} of the window", PaneInfos[pane].Name, PaneInfos[pane].Corner);
    return formatLine("the {} panel", PaneInfos[pane].Name);
}

// -- automated tests --------------------------------------------------------------------------------------------------
//
//  The automated tests live in their own executable, onyx-tests (the tests/ folder). It runs tests in worker processes
//  of their own, because tests start and stop Onyx, open windows and may crash. The control center lists them,
//  lets you pick, and runs the runner with --format events: one tab separated line per event ("@onyx-test", kind,
//  fields), read by a background thread and applied here once per frame. CI runs the very same executable.

#ifndef ONYX_TESTS_EXECUTABLE
#    define ONYX_TESTS_EXECUTABLE ""
#endif

#ifdef _WIN32
#    define onyxPopen _popen
#    define onyxPclose _pclose
#else
#    define onyxPopen popen
#    define onyxPclose pclose
#endif

constexpr const char *TestMarker = "@onyx-test";

constexpr const char *TestBadges[Test_StateCount] = {"-",    "QUEUED", "RUN",     "PASS", "FAIL",
                                                     "SKIP", "CRASH",  "TIMEOUT", "KNOWN"};
const Onyx::Color TestColors[Test_StateCount] = {Theme::Dim, Theme::Dim,  Theme::Info, Theme::Pass, Theme::Fail,
                                                 Theme::Dim, Theme::Fail, Theme::Fail, Theme::Todo};
constexpr const char *TestMeanings[Test_StateCount] = {
    "Not run yet",
    "Waiting for its turn",
    "Running right now, in a worker process",
    "Passed",
    "Failed: an expectation did not hold",
    "Skipped: this machine cannot run it (no audio device, a platform without the feature)",
    "Crashed: an assert, a validation error or a signal ended its process",
    "Timed out: the watchdog ended it",
    "Known issue: it fails because of a reported bug, and does not fail the run"};

static bool isBadTest(const TestState state)
{
    return state == Test_Fail || state == Test_Crash || state == Test_Timeout;
}

static void trimLineEnd(String &line)
{
    while (!line.IsEmpty() && (line[line.GetSize() - 1] == '\n' || line[line.GetSize() - 1] == '\r'))
        line.Pop();
}

// an event line: the marker, then tab separated fields with \\, \t, \n and \r escaped (tests/framework/protocol.hpp).
// fields[0] is the kind
static bool parseEvent(const String &line, Array<String> &fields)
{
    fields.Clear();
    const String marker = String::Format("{}\t", TestMarker);
    const usize at = line.Find(marker.CString());
    if (at == String::npos)
        return false;
    String field{};
    for (usize i = at + marker.GetSize(); i <= line.GetSize(); ++i)
    {
        if (i == line.GetSize() || line[i] == '\t')
        {
            fields.Append(field);
            field.Clear();
            continue;
        }
        if (line[i] != '\\' || i + 1 == line.GetSize())
        {
            field += line[i];
            continue;
        }
        const char c = line[++i];
        field += c == 't' ? '\t' : (c == 'n' ? '\n' : (c == 'r' ? '\r' : c));
    }
    return !fields.IsEmpty();
}

static String testsCommand(const TestRunner &tr, const TKit::StringView arguments)
{
    String command = String::Format("\"{}\" {} 2>&1", tr.Executable, arguments);
#ifdef _WIN32
    // cmd /c strips the outer quotes of the whole line: wrap it once more so the quoted executable path survives
    command = String::Format("\"{}\"", command);
#endif
    return command;
}

static TestEntry *findTest(TestRunner &tr, const String &id)
{
    for (TestEntry &test : tr.Tests)
        if (test.Id == id)
            return &test;
    return nullptr;
}

static bool isSuiteOpen(const TestRunner &tr, const String &suite)
{
    for (const String &open : tr.OpenSuites)
        if (open == suite)
            return true;
    return false;
}
static void setSuiteOpen(TestRunner &tr, const String &suite, const bool open)
{
    for (u32 i = 0; i < tr.OpenSuites.GetSize(); ++i)
        if (tr.OpenSuites[i] == suite)
        {
            if (!open)
                tr.OpenSuites.RemoveOrdered(tr.OpenSuites.begin() + i);
            return;
        }
    if (open)
        tr.OpenSuites.Append(suite);
}

// asks the runner for its tests. fast: listing does not initialize Onyx. selections and results of tests that are
// still there survive a refresh
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4996)
static void listTests(TestRunner &tr)
{
    if (tr.Running.load())
        return;
    tr.Problem.Clear();
    // no environment access in Toolkit
    if (const char *env = std::getenv("ONYX_TESTS"); env && *env)
        tr.Executable = env;
    else
        tr.Executable = ONYX_TESTS_EXECUTABLE;
    if (tr.Executable.IsEmpty())
    {
        tr.Problem =
            "The tests were not built: configure with ONYX_BUILD_TESTS=ON, or point the ONYX_TESTS environment "
            "variable to an onyx-tests executable.";
        tr.Tests.Clear();
        return;
    }

    std::FILE *pipe = onyxPopen(testsCommand(tr, "--list --format events").CString(), "r");
    if (!pipe)
    {
        tr.Problem = String::Format("Could not start {}", tr.Executable);
        return;
    }
    Array<TestEntry> tests{};
    Array<String> f{};
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe))
    {
        String line{static_cast<const char *>(buffer)};
        trimLineEnd(line);
        // test: id suite description known-issue file line
        if (!parseEvent(line, f) || f.GetSize() < 4 || f[0] != "test")
            continue;
        TestEntry entry{};
        entry.Id = f[1];
        entry.Suite = f[2];
        entry.Description = f[3];
        if (f.GetSize() > 4)
            entry.KnownIssue = f[4];
        if (f.GetSize() > 6)
            entry.Where = String::Format("{}:{}", f[5], f[6]);
        if (const TestEntry *old = findTest(tr, entry.Id))
        {
            entry.Selected = old->Selected;
            entry.State = old->State;
            entry.Milliseconds = old->Milliseconds;
            entry.Failures = old->Failures;
            entry.Crash = old->Crash;
            entry.Notes = old->Notes;
            entry.Output = old->Output;
        }
        tests.Append(std::move(entry));
    }
    const int code = onyxPclose(pipe);
    tr.Tests = std::move(tests);
    if (tr.Tests.IsEmpty())
        tr.Problem = String::Format("No tests listed by {} (exit status {})", tr.Executable, code);
}
TKIT_COMPILER_WARNING_IGNORE_POP()

using TestPicks = Array<const TestEntry *>;

static bool isPicked(const TestPicks &picked, const TestEntry *test)
{
    for (const TestEntry *p : picked)
        if (p == test)
            return true;
    return false;
}

// whole suites go by name, the rest by id, so the command line stays short (Windows caps it)
static String selectionArgument(const TestRunner &tr, const TestPicks &picked)
{
    String argument{};
    Array<String> suitesDone{};
    for (const TestEntry *test : picked)
    {
        bool wholeSuite = true;
        for (const TestEntry &other : tr.Tests)
            if (other.Suite == test->Suite)
                wholeSuite &= isPicked(picked, &other);
        const String *item = &test->Id;
        if (wholeSuite)
        {
            bool done = false;
            for (const String &suite : suitesDone)
                done |= suite == test->Suite;
            if (done)
                continue;
            suitesDone.Append(test->Suite);
            item = &test->Suite;
        }
        if (!argument.IsEmpty())
            argument += ',';
        argument += *item;
    }
    return argument;
}

static void startTests(App &app, const TestPicks &picked, const String &label)
{
    TestRunner &tr = app.Tests;
    if (tr.Running.load() || picked.IsEmpty())
        return;
    if (tr.Reader.joinable())
        tr.Reader.join();

    for (TestEntry &test : tr.Tests)
        if (isPicked(picked, &test))
        {
            test.State = Test_Queued;
            test.Milliseconds = 0;
            test.Failures.Clear();
            test.Crash.Clear();
            test.Notes.Clear();
            test.Output.Clear();
        }

    tr.Planned = picked.GetSize();
    tr.Finished = 0;
    tr.Counts = {};
    tr.StartedAt = app.Time;
    tr.Seconds = 0.f;
    tr.HasRun = true;
    tr.Label = label;
    tr.Running.store(true);

    String arguments = String::Format("--format events --jobs {}", tr.Jobs);
    if (tr.Speed != Speed_Fast)
        arguments += String::Format(" --speed {}", SpeedNames[tr.Speed]);
    arguments += String::Format(" {}", selectionArgument(tr, picked));
    tr.Command = testsCommand(tr, arguments);
    tr.Pending.Clear();
    tr.PipedSize = 0;
    tr.Reader = std::thread{[&tr] {
        // hands bytes over to the main thread, waiting for room when it has not taken the last ones out yet
        const auto pipeOut = [&tr](const char *data, const usz size) {
            for (;;)
            {
                {
                    const std::lock_guard lock{tr.Mutex};
                    if (tr.PipedSize + size <= tr.Piped.GetSize())
                    {
                        std::memcpy(tr.Piped.GetData() + tr.PipedSize, data, size);
                        tr.PipedSize += size;
                        return;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        std::FILE *pipe = onyxPopen(tr.Command.CString(), "r");
        if (!pipe)
        {
            // an event of this file's own, in the runner's format
            constexpr const char *problem = "@onyx-test\tproblem\tCould not start the test runner\n";
            pipeOut(problem, std::strlen(problem));
            tr.Running.store(false);
            return;
        }
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe))
            pipeOut(buffer, std::strlen(buffer));
        const int code = onyxPclose(pipe);
        const std::lock_guard lock{tr.Mutex};
        tr.ExitCode = code;
        tr.Running.store(false);
    }};
}
static void startTests(App &app, const TestPicks &picked, const char *label)
{
    startTests(app, picked, String{label});
}

static TestState stateFromName(const String &name)
{
    constexpr const char *names[] = {"pass", "fail", "skip", "crash", "timeout", "known"};
    constexpr TestState states[] = {Test_Pass, Test_Fail, Test_Skip, Test_Crash, Test_Timeout, Test_Known};
    for (u32 i = 0; i < 6; ++i)
        if (name == names[i])
            return states[i];
    return Test_Crash;
}

// what one line of the runner says
static void applyTestEvent(TestRunner &tr, const String &line)
{
    Array<String> f{};
    if (!parseEvent(line, f) || f.GetSize() < 2)
        return;
    const String &kind = f[0];
    if (kind == "problem")
    {
        tr.Problem = f[1];
        return;
    }
    if (kind == "plan" || kind == "summary")
        return;
    TestEntry *test = findTest(tr, f[1]);
    if (!test)
        return;
    if (kind == "begin")
        test->State = Test_Running;
    else if (kind == "fail" && f.GetSize() >= 6)
        test->Failures.Append(TestFailure{f[2], f[3], f[4], f[5]});
    else if (kind == "skip" && f.GetSize() >= 3)
        test->Notes.Append(String::Format("Skipped: {}", f[2]));
    else if (kind == "note" && f.GetSize() >= 3)
        test->Notes.Append(f[2]);
    else if (kind == "crash" && f.GetSize() >= 3)
        test->Crash = f[2];
    else if (kind == "output" && f.GetSize() >= 3)
        test->Output.Append(f[2]);
    else if (kind == "end" && f.GetSize() >= 4)
    {
        test->State = stateFromName(f[2]);
        test->Milliseconds = u32(std::strtoul(f[3].CString(), nullptr, 10));
        ++tr.Counts[test->State];
        ++tr.Finished;
    }
}

// applies what the runner reported since last frame
static void updateTests(App &app)
{
    TestRunner &tr = app.Tests;
    // read before the bytes are taken: the reader clears it after its last ones are in, so a run seen as over here has
    // nothing left to say
    const bool running = tr.Running.load();
    {
        const std::lock_guard lock{tr.Mutex};
        const TKit::StringView piped{tr.Piped.GetData(), usize(tr.PipedSize)};
        if (!piped.IsEmpty())
            tr.Pending += String{piped};
        tr.PipedSize = 0;
    }
    usize start = 0;
    for (usize end = tr.Pending.Find('\n'); end != String::npos; end = tr.Pending.Find('\n', start))
    {
        String line = tr.Pending.SubString(start, end - start);
        trimLineEnd(line);
        applyTestEvent(tr, line);
        start = end + 1;
    }
    if (start != 0)
        tr.Pending = tr.Pending.SubString(start);
    // a runner that dies half way through a line still said something
    if (!running && !tr.Pending.IsEmpty())
    {
        applyTestEvent(tr, tr.Pending);
        tr.Pending.Clear();
    }

    if (running)
        tr.Seconds = app.Time - tr.StartedAt;
    else if (tr.Reader.joinable())
    {
        tr.Reader.join();
        // a test still marked queued or running never reported an end: the runner itself died
        for (TestEntry &test : tr.Tests)
            if (test.State == Test_Queued || test.State == Test_Running)
            {
                test.State = Test_Crash;
                test.Crash = "The test runner ended before this test reported";
                ++tr.Counts[Test_Crash];
            }
        // what failed is what to look at: unfold its suites
        for (const TestEntry &test : tr.Tests)
            if (isBadTest(test.State))
                setSuiteOpen(tr, test.Suite, true);
    }
}

// a failed test as plain text: where each check failed, what it found, the messages and how its process ended
static void appendFailures(String &out, const TestEntry &test, const char *indent)
{
    for (const TestFailure &failure : test.Failures)
    {
        out += String::Format("{}{}: {}\n", indent, failure.Where, failure.Expression);
        forEachLine(failure.Expansion, [&](const TKit::StringView part) {
            if (!part.IsEmpty())
                out += String::Format("{}  With expansion: {}\n", indent, part);
        });
        forEachLine(failure.Info, [&](const TKit::StringView part) {
            if (!part.IsEmpty())
                out += String::Format("{}  {}\n", indent, part);
        });
    }
    if (!test.Crash.IsEmpty())
        out += String::Format("{}{}\n", indent, test.Crash);
}

// -- report ---------------------------------------------------------------------------------------------------------

// a plain text summary of the last automated run and of the visual review, ready to paste into a chat or an issue
static void copyReport(App &app)
{
    const Graph::RenderBackendInfo rinfo = Graph::GetRenderBackendInfo();
    const Graph::PlatformBackendInfo pinfo = Graph::GetPlatformBackendInfo();
    const f32v2 scale = app.Window->GetContentScale();
    const u32v2 size = app.Window->GetSize();
    const TestRunner &tr = app.Tests;

    String report{};
    report += String::Format("Onyx control center report\n{} ({}) | {} | {} layout | scale {:.2f} | window {} "
                             "x {}\n",
                             rinfo.Device.Name, rinfo.Device.DriverName, PlatformNames[pinfo.Platform],
                             app.Mode == Mode_Direct ? "direct" : "docked", scale[0], size[0], size[1]);

    if (!tr.HasRun)
        report += "\nAutomated tests: not run in this session\n";
    else
    {
        report += String::Format("\nAutomated tests ({}): {} of {} done, {} passed, {} failed, {} crashed, "
                                 "{} timed out, {} skipped, {} known "
                                 "issues, {:.1f} s\n",
                                 tr.Label, tr.Finished, tr.Planned, tr.Counts[Test_Pass], tr.Counts[Test_Fail],
                                 tr.Counts[Test_Crash], tr.Counts[Test_Timeout], tr.Counts[Test_Skip],
                                 tr.Counts[Test_Known], tr.Seconds);
        for (const TestEntry &test : tr.Tests)
        {
            if (!isBadTest(test.State) && test.State != Test_Known)
                continue;
            report += String::Format("- {} {}\n", TestBadges[test.State], test.Id);
            appendFailures(report, test, "    ");
            if (test.State == Test_Known)
                report += String::Format("    Known issue: {}\n", test.KnownIssue);
        }
    }

    u32 verdicts[Verdict_Count]{};
    for (const Category &cat : app.Categories)
        for (const VisualCheck &check : cat.Visuals)
            verdicts[check.Mark] += isApplicable(app, check);
    report +=
        String::Format("\nVisual review: {} right, {} broken, {} skipped, {} not judged\n", verdicts[Verdict_Good],
                       verdicts[Verdict_Broken], verdicts[Verdict_Skipped], verdicts[Verdict_Unseen]);
    for (const Verdict verdict : {Verdict_Broken, Verdict_Skipped, Verdict_Unseen})
        for (const Category &cat : app.Categories)
            for (const VisualCheck &check : cat.Visuals)
                if (check.Mark == verdict && isApplicable(app, check))
                    report += String::Format("- {} [{}] {}\n", VerdictBadges[verdict], cat.Title, check.Name);

    Graph::SetClipboard(report.CString());
    app.ReportLines = 0;
    for (const char c : report)
        app.ReportLines += c == '\n';
    app.ReportCopiedAt = app.Time;
}

// -- tests window ---------------------------------------------------------------------------------------------------

static bool matchesFilter(const TestRunner &tr, const TestEntry &test)
{
    if (tr.OnlyProblems && !isBadTest(test.State) && test.State != Test_Known)
        return false;
    return tr.Filter[0] == '\0' || test.Id.Contains(tr.Filter);
}

// the overlay's own fold arrows, or plain characters with a font that lacks them
static Onyx::CodePoint foldArrow(const Onyx::Overlay *ui, const bool open)
{
    const Onyx::Resource font = ui->GetMainNativeWindow()->Context->GetState().Font;
    const Onyx::CodePoint arrow = open ? Onyx::CodePoint_ArrowDown : Onyx::CodePoint_ArrowRight;
    return Onyx::Font_HasGlyph(font, arrow) ? arrow : Onyx::CodePoint(open ? 'v' : '>');
}

// what went wrong, the way the runner reports it: for each failed check its source line and expression, the values
// it saw, and the messages in force (CHECK_MSG, INFO, the overlay test engine's last actions). then how the process
// ended and the last lines it printed
static void drawFailures(Onyx::Overlay *ui, const TestEntry &test, const u32 maxOutputLines)
{
    for (const TestFailure &failure : test.Failures)
    {
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        text(ui, Theme::Dim, failure.Where);
        if (!failure.Expression.IsEmpty())
            text(ui, Theme::Fail, failure.Expression);
        ui->PopPanel();
        forEachLine(failure.Expansion, [ui](const TKit::StringView part) {
            if (!part.IsEmpty())
                field(ui, "    With expansion ", part);
        });
        forEachLine(failure.Info, [ui](const TKit::StringView part) {
            if (!part.IsEmpty())
                text(ui, part.StartsWith("Engine: ") ? Theme::Dim : Theme::Text, formatLine("    {}", part).View(),
                     Onyx::TextMode_Wrapped);
        });
    }
    if (!test.Crash.IsEmpty())
        text(ui, Theme::Fail, test.Crash, Onyx::TextMode_Wrapped);
    for (const String &note : test.Notes)
        text(ui, Theme::Dim, note, Onyx::TextMode_Wrapped);
    if (!test.Output.IsEmpty() && ui->PushTree(formatLine("Output ({} lines)###output", test.Output.GetSize()).View()))
    {
        const u32 size = Math::Min(maxOutputLines, test.Output.GetSize());
        ui->BeginScroll("Output", 320.f, Onyx::OverlayScrollFlag_Borders);
        for (u32 i = 0; i < size; ++i)
            text(ui, Theme::Dim, test.Output[i], Onyx::TextMode_Wrapped);
        ui->EndScroll();
        ui->PopTree();
    }
}

static void drawTestRow(App &app, TestEntry &test, const bool running, const u32 maxOutputLines)
{
    Onyx::Overlay *ui = app.Ui;
    ui->PushId(Onyx::LayoutId{&test});
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    ui->BeginDisabled(running);
    ui->CheckBox("##selected", &test.Selected);
    ui->EndDisabled();
    badge(ui, TestBadges[test.State], TestColors[test.State]);
    // the buttons before the id: a long id would push them out of the window
    ui->BeginDisabled(running);
    if (ui->Button("Run##run", Onyx::OverlayButtonFlag_Small))
    {
        TestPicks one{};
        one.Append(&test);
        startTests(app, one, test.Id);
    }
    ui->EndDisabled();
    tooltip(ui, Theme::Accent, "Runs this test alone", "");
    if (isBadTest(test.State) || test.State == Test_Known)
    {
        if (ui->Button("Copy##copy", Onyx::OverlayButtonFlag_Small))
        {
            String report = String::Format("{} {}\n", TestBadges[test.State], test.Id);
            appendFailures(report, test, "  ");
            for (const String &line : test.Output)
                report += String::Format("  | {}\n", line);
            Graph::SetClipboard(report.CString());
        }
        tooltip(ui, Theme::Accent, "Copies what went wrong",
                "Every failed check with its values and messages, how the process ended and the last lines it printed, "
                "as plain text.");
    }
    text(ui, Theme::Text, test.Id);
    String more = String::Format("{}", test.Description);
    if (!test.Where.IsEmpty())
        more += String::Format("\n\nDeclared at {}", test.Where);
    if (!test.KnownIssue.IsEmpty())
        more += String::Format("\n\nKnown issue: {}", test.KnownIssue);
    tooltip(ui, TestColors[test.State], TestMeanings[test.State], more);
    if (test.Milliseconds != 0)
        text(ui, Theme::Dim, formatLine("{} ms", test.Milliseconds).View());
    ui->PopPanel();
    drawFailures(ui, test, maxOutputLines);
    ui->PopId();
}

// a suite's row: its fold arrow, a checkbox that selects every test in it (folded or not), its counts and a button
// that runs just this suite
static void drawSuite(App &app, const u32 begin, const u32 end, const bool running)
{
    Onyx::Overlay *ui = app.Ui;
    TestRunner &tr = app.Tests;
    const String &suite = tr.Tests[begin].Suite;

    u32 shown = 0;
    u32 bad = 0;
    u32 done = 0;
    bool allSelected = true;
    TestPicks tests{};
    for (u32 i = begin; i < end; ++i)
    {
        const TestEntry &test = tr.Tests[i];
        shown += matchesFilter(tr, test);
        bad += isBadTest(test.State);
        done += test.State >= Test_Pass;
        allSelected &= test.Selected;
        tests.Append(&test);
    }
    if (shown == 0)
        return;

    ui->PushId(Onyx::LayoutId{suite});
    bool open = isSuiteOpen(tr, suite);
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button(
            Onyx::OverlayLabel{Onyx::LayoutId{"fold"}, Onyx::OverlayTitle{TKit::StringView{}, foldArrow(ui, open)}},
            Onyx::OverlayButtonFlag_Small))
    {
        open = !open;
        setSuiteOpen(tr, suite, open);
    }
    ui->BeginDisabled(running);
    if (ui->Button("Run suite##run", Onyx::OverlayButtonFlag_Small))
        startTests(app, tests, suite);

    bool select = allSelected;
    if (ui->CheckBox("##suite", &select))
        for (u32 i = begin; i < end; ++i)
            tr.Tests[i].Selected = select;
    ui->EndDisabled();
    tooltip(ui, Theme::Accent, "Selects the whole suite", "Every test in it, whether the suite is folded or not.");
    if (bad != 0)
        badge(ui, "FAIL", Theme::Fail);
    else if (done == end - begin)
        badge(ui, "PASS", Theme::Pass);
    text(ui, Theme::Text, suite);
    text(ui, Theme::Dim,
         formatLine("{} tests{}{}", end - begin, done ? formatLine(", {} ran", done).View() : TKit::StringView{},
                    bad ? formatLine(", {} failing", bad).View() : TKit::StringView{})
             .View());
    ui->PopPanel();

    if (open)
        for (u32 i = begin; i < end; ++i)
            if (matchesFilter(tr, tr.Tests[i]))
                drawTestRow(app, tr.Tests[i], running, tr.OutputLines);
    ui->PopId();
}

static void drawAutomatedTests(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    TestRunner &tr = app.Tests;
    const bool running = tr.Running.load();

    text(ui, Theme::Dim,
         "Everything a program can check runs in onyx-tests, the same executable CI runs: tests run in worker "
         "processes, so a crash or an assert only takes its own test down. Tests open small windows while they run.",
         Onyx::TextMode_Wrapped);

    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button("Refresh list"))
        listTests(tr);
    tooltip(ui, Theme::Accent, "Lists the tests again",
            "After rebuilding onyx-tests. Selections and results of the tests that are still there are kept.");
    if (!tr.Executable.IsEmpty())
        text(ui, Theme::Dim, tr.Executable);
    ui->PopPanel();
    if (!tr.Problem.IsEmpty())
    {
        text(ui, Theme::Fail, tr.Problem, Onyx::TextMode_Wrapped);
        if (tr.Tests.IsEmpty())
            return;
    }

    // -- run controls -------------------------------------------------------------------------------------------------
    TestPicks all{};
    TestPicks selected{};
    TestPicks problems{};
    for (const TestEntry &test : tr.Tests)
    {
        all.Append(&test);
        if (test.Selected)
            selected.Append(&test);
        if (isBadTest(test.State))
            problems.Append(&test);
    }

    ui->BeginDisabled(running);
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button(formatLine("Run all ({})###run all", all.GetSize()).View()))
        startTests(app, all, "all");
    if (ui->Button(formatLine("Run selected ({})###run selected", selected.GetSize()).View()))
        startTests(app, selected, "selected");
    if (ui->Button(formatLine("Run failed ({})###run failed", problems.GetSize()).View()))
        startTests(app, problems, "failed");
    if (ui->Button("Clear selection"))
        for (TestEntry &test : tr.Tests)
            test.Selected = false;
    ui->PopPanel();
    ui->HorizontalSlider("Tests at a time", &tr.Jobs, 1u, 16u);
    ui->HorizontalSlider("Maximum output lines", &tr.OutputLines, 20u, 1600u);
    ui->EndDisabled();
    tooltip(ui, Theme::Accent, "How many test processes run in parallel",
            "More is faster. Timing sensitive tests (frame pacing) are more reliable with fewer.");

    ui->BeginDisabled(running);
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    text(ui, Theme::Dim, "Overlay tests play at");
    ui->RadioButton("Fast", &tr.Speed, u32(Speed_Fast));
    ui->RadioButton("Normal", &tr.Speed, u32(Speed_Normal));
    ui->RadioButton("Cinematic", &tr.Speed, u32(Speed_Cinematic));
    ui->PopPanel();
    ui->EndDisabled();
    tooltip(ui, Theme::Accent, "How fast the overlay test engine plays the user",
            "Fast moves the mouse as quickly as frames go. Normal and cinematic move it like a person would and draw "
            "it, so a test can be watched: run one, or a suite with one test at a time.");

    // -- progress and summary -----------------------------------------------------------------------------------------
    if (tr.HasRun)
    {
        ui->ProgressBar("##progress", tr.Planned ? f32(tr.Finished) / f32(tr.Planned) : 1.f,
                        "{} {} of {} ({}), {:.1f} s", running ? "Running" : "Ran", tr.Finished, tr.Planned, tr.Label,
                        tr.Seconds);
        const u32 bad = tr.Counts[Test_Fail] + tr.Counts[Test_Crash] + tr.Counts[Test_Timeout];
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        badge(ui, running ? "RUN" : (bad ? "FAIL" : "PASS"), running ? Theme::Info : (bad ? Theme::Fail : Theme::Pass));
        text(ui, Theme::Text,
             formatLine("{} passed, {} failed, {} crashed, {} timed out, {} skipped, {} known", tr.Counts[Test_Pass],
                        tr.Counts[Test_Fail], tr.Counts[Test_Crash], tr.Counts[Test_Timeout], tr.Counts[Test_Skip],
                        tr.Counts[Test_Known])
                 .View());
        ui->PopPanel();
    }

    // -- list ---------------------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Tests");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    ui->InputText("Filter", tr.Filter, sizeof(tr.Filter), "Part of an id, e.g. overlay. or resize");
    ui->CheckBox("Problems only", &tr.OnlyProblems);
    ui->PopPanel();

    ui->BeginScroll("Test list", 520.f, Onyx::OverlayScrollFlag_Borders);
    u32 i = 0;
    while (i < tr.Tests.GetSize())
    {
        u32 end = i;
        while (end < tr.Tests.GetSize() && tr.Tests[end].Suite == tr.Tests[i].Suite)
            ++end;
        drawSuite(app, i, end, running);
        i = end;
    }
    ui->EndScroll();
}

static void drawVisualReview(App &app, const GuideStep &step)
{
    Onyx::Overlay *ui = app.Ui;
    text(ui, Theme::Dim,
         "What a program cannot judge: crisp text, blending, motion, sound. Each check says what to do in the showcase "
         "and what to look at, and you give the verdict. Verdicts are saved as you give them.",
         Onyx::TextMode_Wrapped);

    // -- next step ----------------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Next");
    if (!step.Visual)
        text(ui, Theme::Pass, formatLine("Every visual check is judged ({} of {}).", step.Done, step.Total).View());
    else
    {
        ui->ProgressBar("##review", step.Total ? f32(step.Done) / f32(step.Total) : 1.f, "{} of {} judged", step.Done,
                        step.Total);
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        badge(ui, "TODO", Theme::Todo);
        swatch(ui, step.Visual->Pane);
        text(ui, Theme::Text, step.Visual->Name);
        ui->PopPanel();
        tooltip(ui, Theme::Todo, VerdictMeanings[Verdict_Unseen], step.Visual->More);
        text(ui, Theme::Text, step.Visual->Do, Onyx::TextMode_Wrapped);
        field(ui, "Look at ", describeWhere(app, *step.Cat, step.Visual->Pane).View());

        ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
        const Verdict verdicts[3] = {Verdict_Good, Verdict_Broken, Verdict_Skipped};
        const char *labels[3] = {"Looks right", "Broken", "Skip"};
        for (u32 i = 0; i < 3; ++i)
            if (ui->Button(labels[i]))
            {
                step.Visual->Mark = verdicts[i];
                app.VerdictsDirty = true;
            }
        if (ui->Button("Open its panel"))
            step.Cat->Open = true;
        if (app.Mode == Mode_Docked)
            tooltip(ui, Theme::Dim, "Opens the category panel",
                    "In the docked layout it opens as a tab in the right column: select its tab there.");
        ui->PopPanel();
    }

    // -- every check, by category -------------------------------------------------------------------------------------
    ui->HorizontalSeparator("All visual checks");
    for (Category &cat : app.Categories)
    {
        if (cat.Visuals.IsEmpty())
            continue;
        u32 todo = 0;
        u32 bad = 0;
        for (const VisualCheck &check : cat.Visuals)
            if (isApplicable(app, check))
            {
                todo += check.Mark == Verdict_Unseen;
                bad += check.Mark == Verdict_Broken;
            }
        ui->PushId(Onyx::LayoutId{&cat});
        if (ui->PushTree(formatLine("{} ({} to judge{})###review", cat.Title, todo, bad ? ", broken ones" : "").View()))
        {
            for (VisualCheck &check : cat.Visuals)
                app.VerdictsDirty |= drawVisualCheck(ui, check, isApplicable(app, check));
            ui->PopTree();
        }
        ui->PopId();
    }
}

static void drawTests(App &app, const GuideStep &step)
{
    app.ReviewTabOpen = false;
    if (!app.ShowTests)
        return;
    Onyx::Overlay *ui = app.Ui;
    const f32v2 wsize = f32v2{app.Window->GetSize()};
    const Onyx::OverlayWindowFlags flags = placeWindow(app, f32v2{wsize[0] - 620.f, 16.f}, f32v2{604.f, 760.f});
    if (!ui->BeginWindow(TestsTitle, &app.ShowTests, flags))
        return;

    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button("Copy report"))
        copyReport(app);
    tooltip(ui, Theme::Accent, "Copies a plain text report",
            "The last automated run (failures with their messages, known issues) and every visual verdict, ready to "
            "paste into a chat or an issue.");
    if (app.Time - app.ReportCopiedAt < 3.f)
        text(ui, Theme::Pass, formatLine("Copied {} lines", app.ReportLines).View());
    ui->PopPanel();

    ui->BeginTabBar("Test kinds");
    if (ui->BeginTab("Automated", Onyx::OverlayTabFlag_StartOpen))
    {
        drawAutomatedTests(app);
        ui->EndTab();
    }
    if (ui->BeginTab("Visual review"))
    {
        app.ReviewTabOpen = true;
        drawVisualReview(app, step);
        ui->EndTab();
    }
    ui->EndTabBar();
    ui->EndWindow();
}

static void drawManual(App &app)
{
    if (!app.ShowManual)
        return;
    Onyx::Overlay *ui = app.Ui;
    const f32v2 wsize = f32v2{app.Window->GetSize()};
    const Onyx::OverlayWindowFlags flags = placeWindow(app, f32v2{0.5f * wsize[0] - 280.f, 50.f}, f32v2{560.f, 690.f});
    if (!ui->BeginWindow(ManualTitle, &app.ShowManual, flags))
        return;

    text(ui, Theme::Accent, "What this is");
    text(ui, Theme::Text,
         "A showcase of what Onyx does, in four views and a panel per topic, and the place to test it. The showcase "
         "never judges anything by itself. Testing happens on demand, in the Tests window: automated tests run in "
         "onyx-tests (the same executable CI runs), and visual checks wait for your verdict.",
         Onyx::TextMode_Wrapped);

    ui->HorizontalSeparator("The four views");
    for (u32 i = 0; i < Pane_Count; ++i)
    {
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        swatch(ui, PaneId(i));
        text(ui, PaneInfos[i].Tint, PaneInfos[i].Name);
        text(ui, Theme::Dim, formatLine("({} in the direct layout)", PaneInfos[i].Corner).View());
        ui->PopPanel();
        text(ui, Theme::Text, PaneInfos[i].Shows, Onyx::TextMode_Wrapped);
    }
    text(ui, Theme::Dim, "Every visual check about a view is tagged with that view's tint.", Onyx::TextMode_Wrapped);

    ui->HorizontalSeparator("Badges");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    for (const TestState state : {Test_Pass, Test_Fail, Test_Crash, Test_Known})
        badge(ui, TestBadges[state], TestColors[state]);
    text(ui, Theme::Text, "Automated tests");
    ui->PopPanel();
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    for (u32 i = 0; i < Verdict_Count; ++i)
        badge(ui, VerdictBadges[i], VerdictColors[i]);
    text(ui, Theme::Text, "Visual checks: your verdict");
    ui->PopPanel();
    text(ui, Theme::Dim, "Hover a test or a check to read what it is about.", Onyx::TextMode_Wrapped);

    ui->HorizontalSeparator("A first session");
    const char *steps[] = {
        "1. Open the Tests window and run all the automated tests.",
        "2. Resize the window. The Platform and device panel has fixed sizes.",
        "3. Change the monitor scale, on Hyprland: hyprctl keyword monitor <name>,preferred,auto,1.5",
        "4. Move both cameras: hover a gallery, press W A S D Q E and scroll to zoom.",
        "5. Switch to the docked layout from the Layout menu, and back.",
        "6. Walk the visual review in the Tests window: it points at what to look at, one check at a time.",
    };
    for (const char *line : steps)
        text(ui, Theme::Text, line, Onyx::TextMode_Wrapped);

    if (ui->Button("Open the tests"))
    {
        app.ShowTests = true;
        app.ShowManual = false;
    }
    ui->EndWindow();
}

static void drawFrameTimes(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const TKit::Timespan active = Onyx::GetTargetDeltaTime(app.Window);
    const TKit::Timespan reference = active > TKit::Timespan{} ? active : app.Window->GetMonitorDeltaTime();
    const f32 target = Math::Max(reference.AsSeconds(), 1e-4f);
    const Onyx::Gradient gradient{TKit::Span<const Onyx::Color>(Theme::FrameTimeStops)};
    constexpr f32 height = 44.f;

    ui->BeginPanel({.Direction = Onyx::LayoutDirection_LeftToRight,
                    .Alignment = Onyx::Overlay::BottomLeft,
                    .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fixed(height)},
                    .ChildGap = 1.f});
    const u32 count = app.FrameTimes.GetSize();
    for (u32 i = 0; i < count; ++i)
    {
        const f32 dt = app.FrameTimes[(app.FrameTimeHead + i) % count];
        const f32 t = Math::Clamp(0.5f * dt / target, 0.f, 1.f);
        ui->Panel({.FillColor = gradient.Evaluate(t),
                   .Sizing = Onyx::LayoutSizing::Fixed(f32v2{3.f, Math::Max(2.f, t * height)})});
    }
    ui->EndPanel();
}

static void drawDashboard(App &app, const GuideStep &step)
{
    Onyx::Overlay *ui = app.Ui;
    const f32v2 wsize = f32v2{app.Window->GetSize()};
    const Onyx::OverlayWindowFlags flags = placeWindow(app, f32v2{16.f}, f32v2{390.f, wsize[1] - 32.f});
    if (!ui->BeginWindow(ControlCenterTitle, Onyx::OverlayWindowFlag_MenuBar | flags))
        return;

    TestRunner &tr = app.Tests;
    if (ui->BeginMenuBar())
    {
        if (ui->BeginMenu("Layout"))
        {
            if (ui->MenuItem("Direct: views in window quadrants", app.Mode == Mode_Direct))
                app.RequestedMode = Mode_Direct;
            const bool docking = ui->Flags & Onyx::OverlayFlag_Docking;
            ui->BeginDisabled(!docking);
            if (ui->MenuItem("Docked: every view in its own panel", app.Mode == Mode_Docked) && docking)
                app.RequestedMode = Mode_Docked;
            ui->EndDisabled();
            if (docking)
                tooltip(ui, Theme::Accent, "Panels dock into All panels",
                        "Drag a panel by its tab and drop it in the middle of the whole window's docking guides: it "
                        "becomes "
                        "a full screen tab next to All panels.");
            else
                tooltip(ui, Theme::Accent, "Docking is off",
                        "OverlayFlag_Docking was turned off (the overlay demo's Configuration section can do it). Turn "
                        "it back on to use the docked layout.",
                        Onyx::OverlayFocusFlag_AllowBlockedByDisabled);
            ui->EndMenu();
        }
        if (ui->BeginMenu("Panels"))
        {
            ui->MenuItem(TestsTitle, &app.ShowTests);
            ui->MenuItem(ManualTitle, &app.ShowManual);
            for (Category &cat : app.Categories)
                ui->MenuItem(cat.Title, &cat.Open);
            ui->EndMenu();
        }
        if (ui->BeginMenu("Tests"))
        {
            ui->MenuItem("Tests window", &app.ShowTests);
            ui->BeginDisabled(tr.Running.load() || tr.Tests.IsEmpty());
            if (ui->MenuItem("Run every automated test"))
            {
                TestPicks all{};
                for (const TestEntry &test : tr.Tests)
                    all.Append(&test);
                startTests(app, all, "all");
                app.ShowTests = true;
            }
            ui->EndDisabled();
            if (ui->MenuItem("Copy report to the clipboard"))
                copyReport(app);
            if (ui->MenuItem("Forget every verdict"))
            {
                for (Category &cat : app.Categories)
                    for (VisualCheck &check : cat.Visuals)
                        check.Mark = Verdict_Unseen;
                app.VerdictsDirty = true;
            }
            ui->EndMenu();
        }
        if (ui->BeginMenu("Help"))
        {
            ui->MenuItem(ManualTitle, &app.ShowManual);
            if (ui->MenuItem("Legend and controls"))
                ui->OpenPopup(LegendPopup);
            ui->MenuItem("Overlay demo (Overlay::ShowDemo)", &app.ShowOverlayDemo);
            ui->EndMenu();
        }
        ui->EndMenuBar();
    }

    if (ui->BeginPopup(LegendPopup, Onyx::OverlayWindowFlag_AutoResize))
    {
        for (u32 i = Test_Pass; i < Test_StateCount; ++i)
        {
            ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
            badge(ui, TestBadges[i], TestColors[i]);
            text(ui, Theme::Text, TestMeanings[i]);
            ui->PopPanel();
        }
        for (u32 i = 0; i < Verdict_Count; ++i)
        {
            ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
            badge(ui, VerdictBadges[i], VerdictColors[i]);
            text(ui, Theme::Text, VerdictMeanings[i]);
            ui->PopPanel();
        }
        text(ui, Theme::Dim,
             "W A S D Q E move the camera of the hovered gallery. Space and LCtrl move up and down in 3D, hold LShift "
             "to look around. The mouse wheel zooms.");
        ui->EndPopup();
    }

    // -- header ----------------------------------------------------------------------------------------------------
    const Graph::RenderBackendInfo rinfo = Graph::GetRenderBackendInfo();
    text(ui, Theme::Accent, "ONYX CONTROL CENTER");
    text(ui, Theme::Dim,
         formatLine("{} | {} | {} layout", rinfo.Device.Name, PlatformNames[Graph::GetPlatformBackendInfo().Platform],
                    app.Mode == Mode_Direct ? "direct" : "docked")
             .View());

    // -- tests -------------------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Tests");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (tr.Running.load())
    {
        badge(ui, "RUN", Theme::Info);
        text(ui, Theme::Text, formatLine("Automated: {} of {} done", tr.Finished, tr.Planned).View());
    }
    else if (tr.HasRun)
    {
        const u32 bad = tr.Counts[Test_Fail] + tr.Counts[Test_Crash] + tr.Counts[Test_Timeout];
        badge(ui, bad ? "FAIL" : "PASS", bad ? Theme::Fail : Theme::Pass);
        text(ui, Theme::Text,
             formatLine("Automated: {} passed, {} failing, {} known", tr.Counts[Test_Pass], bad, tr.Counts[Test_Known])
                 .View());
    }
    else
    {
        badge(ui, "-", Theme::Dim);
        text(ui, Theme::Dim,
             tr.Tests.IsEmpty() ? "Automated: not available"
                                : formatLine("Automated: {} tests, not run", tr.Tests.GetSize()).View());
    }
    ui->PopPanel();
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    u32 broken = 0;
    for (const Category &cat : app.Categories)
        for (const VisualCheck &check : cat.Visuals)
            broken += isApplicable(app, check) && check.Mark == Verdict_Broken;
    const Verdict summary = broken ? Verdict_Broken : (step.Visual ? Verdict_Unseen : Verdict_Good);
    badge(ui, VerdictBadges[summary], VerdictColors[summary]);
    text(ui, Theme::Text, formatLine("Visual: {} of {} judged, {} broken", step.Done, step.Total, broken).View());
    ui->PopPanel();
    if (app.LoadedVerdicts != 0)
        text(ui, Theme::Dim, formatLine("{} verdicts loaded from {}", app.LoadedVerdicts, VerdictFile).View());
    if (!app.ShowTests && ui->Button("Open the tests"))
        app.ShowTests = true;

    // -- frame timing ----------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Frame timing");
    f32 avg = 0.f;
    f32 worst = 0.f;
    for (const f32 dt : app.FrameTimes)
    {
        avg += dt;
        worst = Math::Max(worst, dt);
    }
    avg /= f32(app.FrameTimes.GetSize());
    text(ui, Theme::Info,
         formatLine("{:.0f} fps   avg {:.2f} ms   worst {:.2f} ms   monitor {:.2f} ms   target {:.2f} ms",
                    avg > 0.f ? 1.f / avg : 0.f, 1000.f * avg, 1000.f * worst,
                    app.Window->GetMonitorDeltaTime().AsMilliseconds(),
                    Onyx::GetTargetDeltaTime(app.Window).AsMilliseconds())
             .View());
    drawFrameTimes(app);
    tooltip(ui, Theme::Accent, "The last frames",
            "Green is on the target delta time (the monitor's refresh interval without one), red is twice as slow or "
            "worse.");

    // -- categories ------------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Showcase panels");
    for (Category &cat : app.Categories)
    {
        u32 todo = 0;
        u32 bad = 0;
        for (const VisualCheck &check : cat.Visuals)
            if (isApplicable(app, check))
            {
                todo += check.Mark == Verdict_Unseen;
                bad += check.Mark == Verdict_Broken;
            }
        ui->PushId(Onyx::LayoutId{&cat});
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        ui->CheckBox(cat.Title, &cat.Open);
        tooltip(ui, Theme::Accent, cat.Title, cat.Summary);
        if (bad)
            badge(ui, VerdictBadges[Verdict_Broken], VerdictColors[Verdict_Broken]);
        else if (todo)
            badge(ui, VerdictBadges[Verdict_Unseen], VerdictColors[Verdict_Unseen]);
        ui->PopPanel();
        ui->PopId();
    }
    ui->EndWindow();
}

// docked layout only: each gallery shows its render texture filling its panel. the image's rectangle and hover come
// back next frame for updatePanes(), which resizes the texture and maps the mouse into it
static void drawGalleryPanels(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const Onyx::RenderView<D2> *overlay = ui->GetMainNativeWindow()->View;
    const Onyx::OverlayWindowFlags flags = Onyx::OverlayWindowFlag_NoScrollBar |
                                           Onyx::OverlayWindowFlag_NoVerticalScroll |
                                           Onyx::OverlayWindowFlag_NoCloseButton;
    for (u32 i = 0; i < Pane_Count; ++i)
    {
        Pane &pane = app.Panes[i];
        pane.ImageHovered = false;
        if (!ui->BeginWindow(PaneInfos[i].Name, flags))
            continue;
        ui->Image(ui->IdFromStack("image"), *pane.Texture,
                  Onyx::Overlay::LySz2{Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Grow()});
        pane.ImageHovered = ui->IsItemHovered();

        // the layout reports last frame's rectangle in the overlay's world space, by its bottom left corner
        if (const Onyx::LayoutElementQueryInfo *info = ui->QueryLastItem())
        {
            const f32v2 a = overlay->WorldToScreen(info->Position);
            const f32v2 b = overlay->WorldToScreen(info->Position + info->Size);
            pane.ImagePos = Math::Min(a, b);
            pane.ImageSize = Math::Absolute(b - a);
        }
        ui->EndWindow();
    }
}

static void drawCategoryPanels(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    for (u32 i = 0; i < Category_Count; ++i)
    {
        Category &cat = app.Categories[i];
        if (!cat.Open)
            continue;
        const f32 offset = 26.f * f32(i);
        const Onyx::OverlayWindowFlags flags =
            placeWindow(app, f32v2{430.f + offset, 40.f + offset}, f32v2{480.f, 640.f});
        if (!ui->BeginWindow(cat.Title, &cat.Open, flags))
            continue;
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        const Line popup = formatLine("{}##help", cat.Title);
        helpButton(ui, popup.View(), cat.Title, cat.Concepts);
        text(ui, Theme::Dim, cat.Summary);
        ui->PopPanel();
        cat.Panel(app);
        ui->EndWindow();
    }
}

static void panelDpi(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;
    const Graph::Window handle = win->GetHandle();
    const f32v2 scale = win->GetContentScale();
    const u32v2 size = win->GetSize();
    const u32v2 screen = Graph::Window_GetScreenSize(handle);
    const u32v2 pixels = Graph::Window_GetPixelSize(handle);

    ui->HorizontalSeparator("Measured");
    field(ui, "Content scale ", formatLine("{:.3f} x {:.3f}", scale[0], scale[1]).View());
    field(ui, "Logical size ", formatLine("{} x {}", size[0], size[1]).View());
    field(ui, "Screen size (platform units) ", formatLine("{} x {}", screen[0], screen[1]).View());
    field(ui, "Framebuffer size (physical) ", formatLine("{} x {}", pixels[0], pixels[1]).View());
    field(ui, "Swapchain recreations ", formatLine("{}", app.SwapchainRecreations).View());
    // graphite's null handle is all ones, not zero, so a plain truthiness test does not work here
    const Graph::Monitor monitor = win->GetMonitor();
    if (Graph::Monitor_IsHandleValid(monitor))
    {
        const f32v2 ms = Graph::Monitor_GetContentScale(monitor);
        field(ui, "Monitor scale (fullscreen only) ", formatLine("{:.3f} x {:.3f}", ms[0], ms[1]).View());
    }

    ui->HorizontalSeparator("Content scale events");
    if (app.ScaleEvents.IsEmpty())
        text(ui, Theme::Dim, "None yet. Change the monitor scale, or move the window to a monitor with another scale.",
             Onyx::TextMode_Wrapped);
    for (const ScaleEvent &e : app.ScaleEvents)
        text(ui, Theme::Info, formatLine("t = {:7.2f}s  ->  {:.3f} x {:.3f}", e.Time, e.Scale[0], e.Scale[1]).View());
}

constexpr const char *FXAAModes = "Fast#Console#Quality low#Quality medium#Quality high";
constexpr const char *FXAAModeNames[] = {"Fast", "Console", "Quality low", "Quality medium", "Quality high"};
constexpr const char *InspectNames[Inspect_Count] = {
    "Pixel view", "2D gallery",          "3D gallery",         "3D gallery labels", "Textures",
    "Overlay",    "Scaled texture view", "Fixed texture view", "Lab view",
};
constexpr const char *SizingNames[] = {"proportional", "fixed"};

// counts which framebuffer each inspected view rendered with last frame. a view only reaches its other framebuffers
// when the previous frames are still on the GPU, so a bug in one of them shows up rarely
static void trackFramebuffers(App &app)
{
    const Onyx::RenderView<D2> *views[Inspect_Count] = {app.Panes[Pane_Pixel].Frame[app.Mode],
                                                        app.Panes[Pane_Gallery2].Frame[app.Mode],
                                                        nullptr,
                                                        app.Panes[Pane_Gallery3].Frame[app.Mode],
                                                        app.Panes[Pane_Textures].Frame[app.Mode],
                                                        app.Ui->GetMainNativeWindow()->View,
                                                        app.RtScaledView,
                                                        app.RtFixedView,
                                                        app.LabView};
    for (u32 i = 0; i < Inspect_Count; ++i)
    {
        const bool is3 = i == Inspect_Gallery3;
        const Onyx::RenderViewFlags flags = is3 ? app.View3[app.Mode]->GetFlags() : views[i]->GetFlags();
        const u32 index = is3 ? app.View3[app.Mode]->GetAttachmentIndex() : views[i]->GetAttachmentIndex();
        if (!(flags & Onyx::RenderViewFlag_Hidden) && index < app.FramebufferUse[i].GetSize())
            ++app.FramebufferUse[i][index];
    }
}

// one view, collapsed to its name and flags. open, it shows where it renders, its rectangles and how they are sized,
// its framebuffer use and the mouse in its coordinates
template <Onyx::Dimension D>
static void inspectView(App &app, const InspectedView slot, const Onyx::RenderView<D> *rv, const char *target,
                        const bool mouseInside, const f32v2 &mouse)
{
    Onyx::Overlay *ui = app.Ui;
    const Onyx::RenderViewFlags flags = rv->GetFlags();
    const Line tags = formatLine("{}{}{}{}{}", flags & Onyx::RenderViewFlag_Shadows ? " shadows" : "",
                                 flags & Onyx::RenderViewFlag_Outlines ? " outlines" : "",
                                 flags & Onyx::RenderViewFlag_Transparency ? " transparency" : "",
                                 flags & Onyx::RenderViewFlag_FXAA ? " fxaa" : "",
                                 flags & Onyx::RenderViewFlag_Hidden ? " hidden" : "");
    // the id after ### keeps the tree open while its title changes
    const Line label =
        formatLine("{}  -{}###inspect{}", InspectNames[slot], tags.Size ? tags.View() : " no flags", u32(slot));
    if (!ui->PushTree(label.View()))
        return;

    const Onyx::Viewport &vp = rv->GetViewport();
    const Onyx::Scissor &sc = rv->GetScissor();
    const u32v2 image = rv->GetImageSize();
    const u32v2 extent = rv->GetImageExtent();
    field(ui, "Renders into ", formatLine("{}, layer {}", target, rv->Layer).View());
    field(ui, "Viewport ",
          formatLine("({:.1f}, {:.1f}) {:.1f} x {:.1f}, {}", vp.Position[0], vp.Position[1], vp.Size[0], vp.Size[1],
                     SizingNames[rv->GetViewportSizing()])
              .View());
    field(ui, "Scissor ",
          formatLine("({:.1f}, {:.1f}) {:.1f} x {:.1f}, {}", sc.Position[0], sc.Position[1], sc.Size[0], sc.Size[1],
                     SizingNames[rv->GetScissorSizing()])
              .View());
    field(ui, "Image ",
          formatLine("{} x {}, {} x {} physical at {:.2f}, {}", image[0], image[1], extent[0], extent[1],
                     rv->GetContentScale()[0], SizingNames[rv->GetImageSizing()])
              .View());
    if (flags & Onyx::RenderViewFlag_FXAA)
        field(ui, "FXAA mode ", TKit::StringView{FXAAModeNames[rv->AntiAliasingMode]});
    const TKit::FixedArray<u32, 4> &use = app.FramebufferUse[slot];
    field(ui, "Frames per framebuffer ", formatLine("{} / {} / {} / {}", use[0], use[1], use[2], use[3]).View());
    tooltip(ui, Theme::Accent, "Counted since the last layout change",
            "A view takes the first framebuffer the GPU is done with, so a capped frame rate mostly uses the first "
            "one. The Immediate present mode keeps several frames in flight and uses them all.");

    if (!mouseInside)
    {
        ui->PopTree();
        return;
    }
    if constexpr (D == D2)
    {
        const f32v2 world = rv->ScreenToWorld(mouse);
        field(ui, "Mouse ",
              formatLine("screen ({:.1f}, {:.1f}), viewport ({:.1f}, {:.1f}), world ({:.3f}, {:.3f})", mouse[0],
                         mouse[1], rv->ScreenToViewport(mouse)[0], rv->ScreenToViewport(mouse)[1], world[0], world[1])
                  .View());
    }
    else
    {
        const f32v3 world = rv->ScreenToWorld(f32v3{mouse, 0.5f});
        field(ui, "Mouse ",
              formatLine("screen ({:.1f}, {:.1f}), world at depth 0.5 ({:.2f}, {:.2f}, {:.2f})", mouse[0], mouse[1],
                         world[0], world[1], world[2])
                  .View());
    }
    ui->PopTree();
}

static void panelViews(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const f32v2 mouse = app.Window->GetMousePosition();
    const char *paneTarget = app.Mode == Mode_Direct ? "the window" : "its panel's render texture";
    const Pane &pixel = app.Panes[Pane_Pixel];
    const Pane &gallery2 = app.Panes[Pane_Gallery2];
    const Pane &gallery3 = app.Panes[Pane_Gallery3];
    const Pane &textures = app.Panes[Pane_Textures];

    ui->HorizontalSeparator("Inspector");
    inspectView(app, Inspect_Pixel, pixel.Frame[app.Mode], paneTarget, pixel.MouseInside, pixel.Mouse);
    inspectView(app, Inspect_Gallery2, gallery2.Frame[app.Mode], paneTarget, gallery2.MouseInside, gallery2.Mouse);
    inspectView(app, Inspect_Gallery3, app.View3[app.Mode], paneTarget, gallery3.MouseInside, gallery3.Mouse);
    inspectView(app, Inspect_Labels3, gallery3.Frame[app.Mode], paneTarget, gallery3.MouseInside, gallery3.Mouse);
    inspectView(app, Inspect_Textures, textures.Frame[app.Mode], paneTarget, textures.MouseInside, textures.Mouse);
    inspectView(app, Inspect_Overlay, app.Ui->GetMainNativeWindow()->View, "the window", app.Window->IsHovered(),
                mouse);
    inspectView(app, Inspect_RtScaled, app.RtScaledView, "the scaled render texture", false, mouse);
    inspectView(app, Inspect_RtFixed, app.RtFixedView, "the fixed render texture", false, mouse);
    inspectView(app, Inspect_Lab, app.LabView, "the window", app.LabMouseInside, mouse);

    ui->HorizontalSeparator("2D gallery camera");
    ui->HorizontalSlider("2D ortho size", &app.GalleryCam.OrthoParameters.Size, 1.f, 40.f);

    ui->HorizontalSeparator("3D gallery camera");
    Onyx::Camera<D3> &cam = app.Cam3;
    ui->DropDown("3D camera mode", &cam.Mode, "Orthographic#Viewport#Perspective");
    tooltip(ui, Theme::Accent, "Orthographic, viewport or perspective",
            "Orthographic shows Size world units from top to bottom, viewport one world unit per logical pixel (so the "
            "gallery looks tiny), perspective a field of view. Near and far bound what is drawn, and the sun's shadow "
            "cascades are split between them.");
    if (cam.Mode == Onyx::CameraMode_Perspective)
    {
        Onyx::PerspectiveParameters &persp = cam.PerspParameters;
        f32 fov = Math::Degrees(persp.FieldOfView);
        if (ui->HorizontalSlider("Field of view (deg)", &fov, 10.f, 150.f, "{:.0f}"))
            persp.FieldOfView = Math::Radians(fov);
        ui->HorizontalSlider("Near##persp", &persp.Near, 0.01f, 5.f, "{:.2f}", 1, Onyx::OverlaySliderFlag_Logarithmic);
        ui->HorizontalSlider("Far##persp", &persp.Far, 5.f, 500.f, "{:.0f}", 1, Onyx::OverlaySliderFlag_Logarithmic);
        persp.Far = Math::Max(persp.Far, persp.Near + 0.01f);
    }
    else
    {
        Onyx::OrthographicParameters<D3> &ortho = cam.OrthoParameters;
        if (cam.Mode == Onyx::CameraMode_Orthographic)
            ui->HorizontalSlider("Ortho size", &ortho.Size, 1.f, 60.f);
        ui->HorizontalSlider("Near##ortho", &ortho.Near, -50.f, 20.f);
        ui->HorizontalSlider("Far##ortho", &ortho.Far, 1.f, 200.f);
        ortho.Far = Math::Max(ortho.Far, ortho.Near + 0.01f);
        tooltip(
            ui, Theme::Accent, "View space depths",
            "Measured along the view direction from the camera, so they can be negative: a negative near also draws "
            "what is behind the camera, which an orthographic camera can see.");
    }
    ui->HorizontalSlider("3D position", &cam.View.Translation, -30.f, 30.f);
    tooltip(
        ui, Theme::Accent, "WASD, space and ctrl move it too",
        "Hover the 3D gallery and hold shift to look around. The scroll wheel zooms: the ortho size in orthographic "
        "mode, the field of view in perspective.");

    if (ui->Button("Reset cameras"))
        resetCameras(app);
}

// every setting of the lab view. its checks compare the view against these
static void panelLab(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;
    LabSettings &lab = app.Lab;
    Onyx::RenderView<D2> *rv = app.LabView;
    const f32v2 wsize = f32v2{win->GetSize()};

    ui->CheckBox("Show the lab view", &lab.Enabled);
    if (app.Mode != Mode_Direct)
    {
        text(ui, Theme::Info, "The lab lives in the direct layout: the docked layout's dockspace covers the window.",
             Onyx::TextMode_Wrapped);
        return;
    }
    if (!lab.Enabled)
        return;

    ui->HorizontalSeparator("Viewport");
    // switching keeps the view where it is, only the units of the sliders change
    bool fixed = lab.FixedViewport;
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->RadioButton("Proportional", &fixed, false);
    ui->RadioButton("Fixed (logical pixels)", &fixed, true);
    ui->PopPanel();
    if (fixed != lab.FixedViewport)
    {
        const Onyx::Viewport &vp = rv->GetViewport();
        lab.Position = fixed ? vp.Position : vp.Position / wsize;
        lab.Size = fixed ? vp.Size : vp.Size / wsize;
        lab.FixedViewport = fixed;
    }
    const f32v2 limit = lab.FixedViewport ? wsize : f32v2{1.f};
    if (lab.FixedViewport)
    {
        ui->HorizontalSlider("Position (px)", &lab.Position, 0.f, Math::Max(wsize[0], wsize[1]), "{:.0f}");
        ui->HorizontalSlider("Size (px)", &lab.Size, 16.f, Math::Max(wsize[0], wsize[1]), "{:.0f}");
    }
    else
    {
        ui->HorizontalSlider("Position", &lab.Position, 0.f, 0.95f);
        ui->HorizontalSlider("Size", &lab.Size, 0.05f, 1.f);
    }
    // kept inside the window, even after it shrinks
    lab.Size = Math::Min(lab.Size, limit);
    lab.Position = Math::Max(Math::Min(lab.Position, limit - lab.Size), f32v2{0.f});

    ui->HorizontalSeparator("Scissor and image");
    ui->HorizontalSlider("Scissor position", &lab.ScissorPosition, 0.f, 0.95f);
    ui->HorizontalSlider("Scissor size", &lab.ScissorSize, 0.05f, 1.f);
    lab.ScissorSize = Math::Min(lab.ScissorSize, 1.f - lab.ScissorPosition);
    tooltip(ui, Theme::Accent, "Fractions of the viewport",
            "Only the part inside the scissor is drawn, the views below show everywhere else.");
    ui->HorizontalSlider("Image scale", &lab.ImageScale, 0.25f, 2.f);
    tooltip(ui, Theme::Accent, "Image size over viewport size",
            "Below 1 the view renders fewer pixels and is stretched, above 1 it supersamples. Nothing may move.");

    ui->HorizontalSeparator("Flags");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->CheckBox("Outlines", &lab.Outlines);
    ui->CheckBox("Transparency", &lab.Transparency);
    ui->CheckBox("FXAA", &lab.FXAA);
    ui->PopPanel();
    if (lab.FXAA)
        ui->DropDown("Lab FXAA mode", &lab.FXAAMode, FXAAModes);
    ui->CheckBox("Transparent background", &lab.TransparentBackground);
    tooltip(ui, Theme::Accent, "Clears to transparent black, like the overlay",
            "The views below must show through the lab, with FXAA on as well.");

    ui->HorizontalSeparator("Camera");
    const bool wasOrthographic = lab.Orthographic;
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->RadioButton("Orthographic", &lab.Orthographic, true);
    ui->RadioButton("Viewport (1 unit = 1 px)", &lab.Orthographic, false);
    ui->PopPanel();
    if (wasOrthographic != lab.Orthographic)
        lab.CameraPosition = f32v2{0.f}; // its units changed
    const f32 step = lab.Orthographic ? 1.f : 50.f;
    if (lab.Orthographic)
        ui->HorizontalSlider("Ortho size", &lab.OrthoSize, 1.f, 20.f);
    ui->HorizontalSlider("Camera position", &lab.CameraPosition, -8.f * step, 8.f * step);
    f32 degrees = Math::Degrees(lab.Rotation);
    if (ui->HorizontalSlider("Camera rotation (deg)", &degrees, -180.f, 180.f, "{:.0f}"))
        lab.Rotation = Math::Radians(degrees);

    ui->HorizontalSeparator("Layer");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button("Above the scene"))
    {
        // the overlay goes back on top right after
        win->BringToTop(rv);
        win->BringToTop(app.Ui->GetMainNativeWindow()->View);
        lab.Placement = LabPlacement_AboveScene;
    }
    if (ui->Button("Below the scene"))
    {
        win->BringToBottom(rv);
        lab.Placement = LabPlacement_BelowScene;
    }
    text(ui, Theme::Dim, formatLine("Layer {}", rv->Layer).View());
    ui->PopPanel();

    ui->HorizontalSeparator("Coordinates");
    if (app.LabMouseInside)
    {
        const f32v2 mouse = win->GetMousePosition();
        const f32v2 local = rv->ScreenToViewport(mouse);
        const f32v2 world = rv->ScreenToWorld(mouse);
        field(ui, "Mouse, screen ", formatLine("({:.1f}, {:.1f})", mouse[0], mouse[1]).View());
        field(ui, "Mouse, viewport ", formatLine("({:.1f}, {:.1f})", local[0], local[1]).View());
        field(ui, "Mouse, world ", formatLine("({:.3f}, {:.3f})", world[0], world[1]).View());
    }
    else
        text(ui, Theme::Dim, "Hover the lab to read the mouse in screen, viewport and world coordinates.",
             Onyx::TextMode_Wrapped);
    const f32v2 cross = rv->WorldToScreen(f32v2{step});
    field(ui, "Green cross ",
          formatLine("world ({:g}, {:g}) -> screen ({:.1f}, {:.1f})", step, step, cross[0], cross[1]).View());
}

static void panelRendering(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    RenderSettings &rs = app.Render;

    ui->HorizontalSeparator("Controls");
    const bool oldOutlines = rs.Outlines;
    ui->CheckBox("Outlines", &rs.Outlines);
    ui->HorizontalSlider("Outline width", &rs.OutlineWidth, 0.f, 0.3f);
    ui->CheckBox("Rotate shapes", &rs.Rotate);
    ui->HorizontalSlider("Alpha (middle row)", &rs.Alpha, 0.05f, 1.f);
    ui->HorizontalSlider("3D alpha (front shapes)", &rs.Alpha3, 0.05f, 1.f);
    ui->HorizontalSlider("Circle hollowness", &rs.Circle.Hollowness, 0.f, 0.95f);
    ui->HorizontalSlider("Circle arc end", &rs.Circle.UpperAngle, 0.1f, 2.f * Math::Pi<f32>());
    ui->HorizontalSlider("Circle inner fade", &rs.Circle.InnerFade, 0.f, 1.f);
    ui->HorizontalSlider("Circle outer fade", &rs.Circle.OuterFade, 0.f, 1.f);

    // view flags go through RenderView::SetFlags, on both layouts' gallery views. the inactive ones stay hidden
    const bool oldShadows = rs.Shadows;
    const bool oldFXAA = rs.FXAA;
    ui->CheckBox("View: shadows", &rs.Shadows);
    ui->CheckBox("View: FXAA", &rs.FXAA);
    tooltip(ui, Theme::Accent, "Anti-aliasing on both galleries",
            "A full screen pass after the outlines. The pixel view never uses it: it must stay pixel perfect.");
    if (rs.FXAA)
        ui->DropDown("FXAA mode", &rs.FXAAMode, FXAAModes);
    if (oldShadows != rs.Shadows || oldFXAA != rs.FXAA || oldOutlines != rs.Outlines)
    {
        Onyx::RenderViewFlags flags = Onyx::RenderViewFlag_Transparency;
        if (rs.Shadows)
            flags |= Onyx::RenderViewFlag_Shadows;
        if (rs.Outlines)
            flags |= Onyx::RenderViewFlag_Outlines;
        if (rs.FXAA)
            flags |= Onyx::RenderViewFlag_FXAA;
        for (u32 m = 0; m < Mode_Count; ++m)
        {
            const Onyx::RenderViewFlags hidden = m == app.Mode ? 0 : Onyx::RenderViewFlag_Hidden;
            app.Panes[Pane_Gallery2].Frame[m]->SetFlags(flags | hidden);
            app.View3[m]->SetFlags(flags | hidden);
        }
    }
    for (u32 m = 0; m < Mode_Count; ++m)
    {
        app.Panes[Pane_Gallery2].Frame[m]->AntiAliasingMode = rs.FXAAMode;
        app.View3[m]->AntiAliasingMode = rs.FXAAMode;
    }
}

static void panelLights(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    LightSettings &ls = app.Lights;

    ui->HorizontalSeparator("2D gallery");
    ui->CheckBox("2D lights", &ls.Lights2);
    ui->CheckBox("Sweep the light around the center occluder", &ls.Sweep);
    tooltip(ui, Theme::Accent, "A repeatable test for the shadow seam",
            "The light orbits the spinning quad. When it sits to the quad's right, the quad is at 180 degrees from it, "
            "where the circular shadow map wraps around: no slit of light may cross the quad or its shadow.");
    text(ui, Theme::Dim, "Without the sweep, hover the 2D gallery: the light follows the cursor.",
         Onyx::TextMode_Wrapped);

    ui->CheckBox("Directional light", &ls.Directional2);
    f32 degrees = Math::Degrees(ls.DirectionalAngle);
    if (ui->HorizontalSlider("Direction (deg)", &degrees, -180.f, 180.f, "{:.0f}"))
        ls.DirectionalAngle = Math::Radians(degrees);

    ui->CheckBox("Cone", &ls.Cone);
    tooltip(ui, Theme::Accent, "Turns the cursor light into a cone",
            "Extent is half of the cone, as a fraction of half a turn. Decay fades the light from the axis to the "
            "edges. Both must stay in [0, 1], an assert enforces it.");
    if (ls.Cone)
    {
        ui->CheckBox("Aim where the light moves", &ls.ConeFollowsMotion);
        if (!ls.ConeFollowsMotion)
        {
            f32 aim = Math::Degrees(ls.ConeAngle);
            if (ui->HorizontalSlider("Aim (deg)", &aim, -180.f, 180.f, "{:.0f}"))
                ls.ConeAngle = Math::Radians(aim);
        }
        ui->HorizontalSlider("Cone width (extent)", &ls.ConeExtent, 0.05f, 1.f);
        ui->HorizontalSlider("Edge softness (decay)", &ls.ConeDecay, 0.f, 1.f);
    }

    ui->DropDown("2D shadow filter", &ls.Filter2, FilterNames);
    tooltip(ui, Theme::Accent, "Hard, PCF or soft",
            "PCF averages several shadow map samples for smooth edges. Soft shadows (PCSS) also widen the edges with "
            "the distance to the occluder and the light's size.");
    if (ls.Filter2 == Filter_Soft)
    {
        ui->HorizontalSlider("2D light size", &ls.LightSize2, 0.01f, 1.f);
        f32 sun = Math::Degrees(ls.SunAngle2);
        if (ui->HorizontalSlider("2D directional angular size (deg)", &sun, 0.1f, 10.f))
            ls.SunAngle2 = Math::Radians(sun);
    }

    ui->HorizontalSeparator("3D gallery");
    ui->CheckBox("3D lights", &ls.Lights3);
    ui->CheckBox("Point light", &ls.Point3);
    ui->CheckBox("Spot light", &ls.Spot3);
    ui->CheckBox("Spot only", &ls.SpotOnly);
    tooltip(ui, Theme::Accent, "Leaves the spot alone",
            "Turns off the directional and point lights, so the spot's light and shadows are unmistakable.");
    ui->CheckBox("Sun straight down", &ls.SunDown);
    tooltip(ui, Theme::Accent, "Points the directional light straight down",
            "A vertical direction is a corner case for the math that builds a light's frame. Alone, the sun must "
            "still light the tops of the shapes, with their shadows right under them.");
    f32 fov = Math::Degrees(ls.SpotFov);
    if (ui->HorizontalSlider("Spot field of view (deg)", &fov, 10.f, 120.f, "{:.0f}"))
        ls.SpotFov = Math::Radians(fov);
    ui->HorizontalSlider("Spot range", &ls.SpotRange, 1.f, 20.f);
    f32 tilt = Math::Degrees(ls.SpotTilt);
    if (ui->HorizontalSlider("Spot tilt (deg)", &tilt, 0.f, 60.f, "{:.0f}"))
        ls.SpotTilt = Math::Radians(tilt);
    f32 turn = Math::Degrees(ls.SpotTurn);
    if (ui->HorizontalSlider("Spot turn (deg)", &turn, -180.f, 180.f, "{:.0f}"))
        ls.SpotTurn = Math::Radians(turn);
    ui->CheckBox("Spot swings", &ls.SpotSwing);
    if (ui->Button("Spot straight down"))
    {
        ls.SpotTilt = 0.f;
        ls.SpotSwing = false;
    }
    tooltip(ui, Theme::Accent, "No tilt, no swing",
            "The same corner case for the spot: the box under it must keep its shadow.");

    ui->DropDown("3D shadow filter", &ls.Filter3, FilterNames);
    if (ls.Filter3 == Filter_Soft)
    {
        ui->HorizontalSlider("Point light size", &ls.PointSize3, 0.01f, 1.f);
        ui->HorizontalSlider("Spot light size", &ls.SpotSize3, 0.01f, 0.5f);
        f32 sun = Math::Degrees(ls.SunAngle3);
        if (ui->HorizontalSlider("Sun angular size (deg)", &sun, 0.1f, 5.f))
            ls.SunAngle3 = Math::Radians(sun);
    }
}

static void panelTextures(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const f32v2 imageSize = f32v2{RenderTextureSize};
    ui->HorizontalSeparator("Sampled in the overlay");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->Image(*app.RtScaled, imageSize);
    ui->Image(*app.RtFixed, imageSize);
    ui->PopPanel();
    text(ui, Theme::Dim, "Left: follows the window scale. Right: fixed at scale 1.");

    if (app.Mode != Mode_Docked)
        return;
    ui->HorizontalSeparator("Docked galleries");
    for (u32 i = 0; i < Pane_Count; ++i)
    {
        const Pane &pane = app.Panes[i];
        const u32v2 size = pane.Texture->GetSize();
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        swatch(ui, PaneId(i));
        field(ui, PaneInfos[i].Name,
              formatLine("  texture {} x {} at {:.2f}, panel {:.0f} x {:.0f}", size[0], size[1],
                         pane.Texture->GetContentScale()[0], pane.ImageSize[0], pane.ImageSize[1])
                  .View());
        ui->PopPanel();
    }
}

static void panelInput(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;
    const InputTracking &in = app.Input;

    ui->HorizontalSeparator("Live state");
    const f32v2 mouse = win->GetMousePosition();
    field(ui, "Mouse (polled) ", formatLine("({:.1f}, {:.1f})", mouse[0], mouse[1]).View());
    if (in.HasEventMouse)
    {
        field(ui, "Mouse (last move event) ",
              formatLine("({:.1f}, {:.1f})", in.EventMouse[0], in.EventMouse[1]).View());
        field(ui, "Event vs polled distance ", formatLine("{:.2f} px", Math::Norm(in.EventMouse - mouse)).View());
        tooltip(ui, Theme::Accent, "Should be about 0 while moving slowly",
                "A constant offset, or a distance that grows with the position by a factor equal to the content "
                "scale, means events and polling use different spaces.");
    }
    field(ui, "Buttons and keys held (events) ",
          formatLine("{} buttons, {} keys", in.HeldButtons.GetSize(), in.HeldKeys.GetSize()).View());
    field(ui, "Last key pressed ", TKit::StringView{in.LastKey});
    field(ui, "Last scroll ", formatLine("({:.2f}, {:.2f})", in.LastScroll[0], in.LastScroll[1]).View());
    field(ui, "Typed text ", formatLine("\"{}\"", TKit::StringView{in.Typed, in.TypedSize}).View());
    field(ui, "Overlay wants ",
          formatLine("mouse {}, scroll {}, keyboard {}", ui->WantCaptureMouse(), ui->WantCaptureScroll(),
                     ui->WantCaptureKeyboard())
              .View());
    field(ui, "Window ", formatLine("hovered {}, fullscreen {}", win->IsHovered(), win->IsFullScreen()).View());

    ui->HorizontalSeparator("Event counters since start");
    for (u32 i = 0; i < Onyx::Event_Count; i += 2)
    {
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        for (u32 j = i; j < i + 2 && j < Onyx::Event_Count; ++j)
        {
            const u32 count = in.EventCounts[j];
            ui->BeginPanel({.Direction = Onyx::LayoutDirection_LeftToRight,
                            .Alignment = Onyx::Overlay::CenterLeft,
                            .Sizing = {Onyx::LayoutSizing::Fixed(210.f), Onyx::LayoutSizing::Fit()},
                            .ChildGap = 6.f});
            text(ui, count ? Theme::Info : Theme::Dim, formatLine("{:>6}", count).View());
            text(ui, count ? Theme::Text : Theme::Dim, EventNames[j]);
            ui->EndPanel();
        }
        ui->PopPanel();
    }
}

static void panelAudio([[maybe_unused]] App &app)
{
    Onyx::Overlay *ui = app.Ui;
#ifndef ONYX_ENABLE_AUDIO
    text(ui, Theme::Info, "Audio is not compiled in. Configure Onyx with ONYX_ENABLE_AUDIO=ON to test it.",
         Onyx::TextMode_Wrapped);
#else
    AudioState &au = app.Audio;
    if (!au.Available)
    {
        text(ui, Theme::Info,
             "No audio output device: Audio_IsAvailable() is false, and Onyx fell back to miniaudio's null backend. "
             "The audio showcase needs one.",
             Onyx::TextMode_Wrapped);
        return;
    }
    field(ui, "Output ",
          formatLine("{} Hz, {} channels", Onyx::Audio_GetSampleRate(), Onyx::Audio_GetChannels()).View());

    ui->HorizontalSeparator("Source");
    ui->DropDown("Wave", &au.Wave, WaveNames);
    ui->HorizontalSlider("Frequency (Hz)", &au.Frequency, 110.f, 880.f, "{:.0f}");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    if (ui->Button("Play"))
        playInstrument(au);
    if (ui->Button("Stop"))
        Onyx::Audio_Stop(au.Instrument);
    ui->CheckBox("Loop", &au.Loop);
    text(ui, Theme::Info,
         formatLine("{}, ended {} times", Onyx::Audio_IsPlaying(au.Instrument) ? "Playing" : "Stopped",
                    au.InstrumentEnds)
             .View());
    ui->PopPanel();
    tooltip(ui, Theme::Accent, "Counted by the main thread end callback",
            "Audio_SetMainThreadEndCallback() increments this every time the sound ends without looping.");
    ui->HorizontalSlider("Volume", &au.Volume, 0.f, 1.f);
    ui->HorizontalSlider("Pitch", &au.Pitch, 0.25f, 4.f);
    ui->HorizontalSlider("Pan", &au.Pan, -1.f, 1.f);
    ui->HorizontalSlider("Master volume", &au.Master, 0.f, 1.f);

    // what the callback produced, most recent last. it moves even with the volume down
    const Synth &synth = au.InstrumentSynth;
    const u32 head = synth.ScopeHead.load(std::memory_order_acquire);
    ui->BeginPanel({.Direction = Onyx::LayoutDirection_LeftToRight,
                    .Alignment = Onyx::Overlay::CenterLeft,
                    .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fixed(60.f)},
                    .ChildGap = 1.f});
    for (u32 i = 0; i < ScopeSize; ++i)
        scopeColumn(ui, synth.Scope[(head + i) % ScopeSize].load(std::memory_order_relaxed), 60.f, Theme::Accent);
    ui->EndPanel();
    tooltip(ui, Theme::Accent, "The scope", "One sample out of every few the callback wrote, kept in a ring buffer.");

    ui->HorizontalSeparator("Spatial source");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->RadioButton("Off", &au.Orbit, Orbit_Off);
    ui->RadioButton("2D orbit", &au.Orbit, Orbit_2D);
    ui->RadioButton("3D orbit", &au.Orbit, Orbit_3D);
    ui->PopPanel();
    text(ui, Theme::Dim,
         "2D: beeps circle the listener at the 2D gallery's center. 3D: they circle the 3D camera, which listens.",
         Onyx::TextMode_Wrapped);

    text(ui, Theme::Dim,
         "Playback state, end callbacks and spatial positions are covered by the audio tests (Tests window).",
         Onyx::TextMode_Wrapped);
#endif
}

static void panelResources(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const Onyx::DefaultResources &res = Onyx::Resources_GetDefault();
    ui->HorizontalSeparator("Counts");
    field(ui, "Dynamic meshes (2D / 3D) ",
          formatLine("{} / {}", Onyx::DynamicMesh_GetCount<D2>(), Onyx::DynamicMesh_GetCount<D3>()).View());
    field(ui, "Font pool ",
          formatLine("{} fonts, {} glyphs", Onyx::FontPool_GetFontCount(res.FontPool),
                     Onyx::FontPool_GetGlyphCount(res.FontPool))
              .View());

    for (u32 dim = 0; dim < 2; ++dim)
    {
        ui->HorizontalSeparator(dim == 0 ? "Frame usage, 2D" : "Frame usage, 3D");
        const UsageMeters &meters = app.Usage[dim];
        for (u32 i = 0; i < Usage_Count; ++i)
        {
            if (!meters.Applies[i])
                continue;
            const f32 fill = usageFill(meters, i);
            ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
            text(ui, Theme::Dim, UsageInfos[i].Name);
            text(ui, fill > 1.f ? Theme::Fail : (fill > 0.75f ? Theme::Todo : Theme::Info),
                 meters.Limit[i] == 0
                     ? formatLine("{} now, {} peak, {} is 0", meters.Now[i], meters.Peak[i], UsageInfos[i].Spec).View()
                     : formatLine("{} now, {} peak of {} {} ({:.0f}%)", meters.Now[i], meters.Peak[i], meters.Limit[i],
                                  UsageInfos[i].Spec, 100.f * fill)
                           .View());
            ui->PopPanel();
        }
    }
    if (ui->Button("Reset peaks"))
        for (UsageMeters &meters : app.Usage)
            meters.Peak = meters.Now;
    tooltip(ui, Theme::Accent, "Measured right before the transfer",
            "From every context, the way the renderer copies them. Open every panel, switch layouts and turn on the "
            "lab to find the real peaks. Draw commands are an upper bound.");

#ifdef TKIT_ENABLE_ENSURE
    if (ui->PushTree("Tier allocator statistics"))
    {
        ui->ShowTierAllocatorStatistics();
        ui->PopTree();
    }
#endif
}

static void panelPlatform(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;
    const Graph::RenderBackendInfo rinfo = Graph::GetRenderBackendInfo();
    const Graph::PlatformBackendInfo pinfo = Graph::GetPlatformBackendInfo();

    ui->HorizontalSeparator("Device");
    field(ui, "GPU ", formatLine("{}", rinfo.Device.Name).View());
    field(ui, "Driver ", formatLine("{} ({})", rinfo.Device.DriverName, rinfo.Device.DriverInfo).View());
    field(ui, "API ",
          formatLine("{}.{}.{}", rinfo.Device.ApiMajor, rinfo.Device.ApiMinor, rinfo.Device.ApiPatch).View());
    field(ui, "Render backend ", formatLine("{}", rinfo.Version).View());
    field(ui, "Platform backend ", formatLine("{} on {}", pinfo.Version, PlatformNames[pinfo.Platform]).View());

    ui->HorizontalSeparator("Monitors");
    for (u32 i = 0; i < Graph::Monitor_GetCount(); ++i)
    {
        const Graph::Monitor monitor = Graph::Monitor_Get(i);
        const Graph::VideoMode mode = Graph::Monitor_GetVideoMode(monitor);
        const f32v2 scale = Graph::Monitor_GetContentScale(monitor);
        const Graph::MonitorArea area = Graph::Monitor_GetWorkArea(monitor);
        text(ui, Theme::Text,
             formatLine("Monitor {}{}", i, monitor == Graph::Monitor_GetPrimary() ? " (primary)" : "").View());
        text(ui, Theme::Info,
             formatLine("    {} x {} @ {} Hz, scale {:.2f}, work area {} x {} at ({}, {})", mode.Size[0], mode.Size[1],
                        mode.RefreshRate, scale[0], area.Size[0], area.Size[1], area.Position[0], area.Position[1])
                 .View());
    }

    // window position and opacity are not supported everywhere (Wayland ignores both), so they are informative only
    ui->HorizontalSeparator("Window");
    const i32v2 pos = win->GetPosition();
    const u32v2 size = win->GetSize();
    field(ui, "Position (not reported on Wayland) ", formatLine("({}, {})", pos[0], pos[1]).View());
    field(ui, "Opacity (read back) ", formatLine("{:.2f}", win->GetOpacity()).View());
    field(ui, "Size ",
          formatLine("{} x {}, last requested {} x {}", size[0], size[1], app.RequestedSize[0], app.RequestedSize[1])
              .View());
    if (ui->DropDown("Present mode", &app.PresentMode, "Immediate#Mailbox#VSync"))
        win->SetPresentMode(Graph::PresentMode(app.PresentMode));

    PacingSettings &pc = app.Pacing;
    pc.Dirty |= ui->DropDown("Frame pacing", &pc.Mode, "Present mode only#Monitor refresh rate#Custom rate");
    tooltip(ui, Theme::Accent, "SetTargetDeltaTime",
            "A target delta time makes Onyx sleep until the window is due. Without one, the present mode alone paces "
            "frames: vsync waits for the monitor, immediate and mailbox run as fast as they can.");
    if (pc.Mode == Pacing_Custom)
        pc.Dirty |= ui->HorizontalSlider("Target rate (Hz)", &pc.Rate, 5u, 360u);
    const TKit::Timespan target = Onyx::GetTargetDeltaTime(win);
    field(ui, "Active target ",
          target > TKit::Timespan{}
              ? formatLine("{:.2f} ms ({} Hz)", target.AsMilliseconds(), Onyx::ToFrequency(target)).View()
              : TKit::StringView{"None"});
    if (pc.Reset && target != pc.Requested)
    {
        text(ui, Theme::Dim,
             "Onyx reset the target to the monitor's: in vsync it does so when the swapchain is recreated or the "
             "window moves.");
        if (ui->Button("Apply again"))
            pc.Dirty = true;
    }
    if (ui->HorizontalSlider("Opacity", &app.Opacity, 0.2f, 1.f))
        win->SetOpacity(app.Opacity);

    ui->HorizontalSeparator("Size and present modes");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    for (const u32v2 &preset : SizePresets)
        if (ui->Button(formatLine("{} x {}", preset[0], preset[1]).View()))
        {
            app.RequestedSize = preset;
            win->SetSize(preset);
        }
    ui->PopPanel();
    tooltip(ui, Theme::Accent, "Resizes the window",
            "A tiling compositor may ignore the request: float the window first. The window and view tests check "
            "resizes on their own.");
    if (ui->Button(app.PresentCycleSteps ? "Cycling present modes..." : "Cycle present modes") &&
        app.PresentCycleSteps == 0)
        startPresentCycle(app);
    tooltip(ui, Theme::Accent, "Steps through the present modes and back",
            "Each change recreates the swapchain. window.present_modes_switch_cleanly checks the same on its own.");
}

// left: the dashboard and the manual as tabs, over the tests window. middle: the four galleries. right: the category
// panels, as tabs. a window that is still docked cannot join a tree (asserted), and one can be left docked: a closed
// panel inside a group the user docked together by hand. so every window of the tree is undocked first, which happens
// at the end of this frame, right before the tree is applied
static void submitDockTree(App &app, const Onyx::LayoutId dock)
{
    Onyx::Overlay *ui = app.Ui;
    TKit::StaticArray<Onyx::LayoutId, Category_Count> panels{};
    for (const Category &cat : app.Categories)
        panels.Append(Onyx::LayoutId{cat.Title});
    const Onyx::LayoutId side[2] = {ControlCenterTitle, ManualTitle};
    const Onyx::LayoutId guide{TestsTitle};
    TKit::FixedArray<Onyx::LayoutId, Pane_Count> galleries{};
    for (u32 i = 0; i < Pane_Count; ++i)
        galleries[i] = Onyx::LayoutId{PaneInfos[i].Name};

    undockPanels(app);

    const Onyx::OverlayDockNode *left =
        Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.62f, Onyx::DockTabBar(TKit::Span<const Onyx::LayoutId>(side)),
                        Onyx::DockTabBar(guide));
    const Onyx::OverlayDockNode *top =
        Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.5f, Onyx::DockTabBar(galleries[Pane_Pixel]),
                        Onyx::DockTabBar(galleries[Pane_Gallery2]));
    const Onyx::OverlayDockNode *bottom =
        Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.5f, Onyx::DockTabBar(galleries[Pane_Gallery3]),
                        Onyx::DockTabBar(galleries[Pane_Textures]));
    const Onyx::OverlayDockNode *scene = Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.5f, top, bottom);
    const Onyx::OverlayDockNode *rest =
        Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.7f, scene, Onyx::DockTabBar(panels));
    ui->SubmitDockTree(dock, Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.24f, left, rest));
}

static void drawOverlay(App &app)
{
    Onyx::Overlay *ui = app.Ui;

    // the docked layout nests two dockspaces, like the editor. a full screen parent, its tab bar hidden, holds a single
    // window: All panels, whose own dockspace holds every panel. a panel dropped on the parent becomes a full screen
    // tab next to All panels. dockspaces must be submitted before any window that docks into them, and every docked
    // session gets fresh ones. the parent undocks its windows once it stops being submitted, but the inner one must
    // not: it is not submitted while another tab of the parent is shown. leaving the docked layout undocks the panels
    // anyway (NoDocking), and the next session undocks the galleries before applying its tree. the trees are submitted
    // once
    if (app.Mode == Mode_Docked)
    {
        const Line parentName = formatLine("Docked layout {}", app.DockSessions);
        const Line hostName = formatLine("All panels {}", app.DockSessions);
        const Line panelsName = formatLine("Docked panels {}", app.DockSessions);
        const Onyx::LayoutId parent{parentName.View()};
        const Onyx::LayoutId host{hostName.View()};
        const Onyx::LayoutId panels{panelsName.View()};
        ui->FullScreenDockSpace(parent,
                                Onyx::OverlayDockNodeFlag_CanBeEmpty | Onyx::OverlayDockNodeFlag_StartWithTabBarHidden,
                                Onyx::OverlayWindowFlag_NoBackground | Onyx::OverlayWindowFlag_MousePassThrough |
                                    Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);
        if (app.DockTreePending)
        {
            ui->DeclareWindow(host);
            ui->DeclareDockSpace(panels, host);
            ui->SubmitDockTree(parent, Onyx::DockTabBar(host));
            submitDockTree(app, panels);
            app.DockTreePending = false;
        }
        ui->PushStyleVar(Onyx::OverlayStyle_ContentAreaPadding, 0.f);
        const bool open =
            ui->BeginWindow(Onyx::OverlayLabel{host, "All panels"}, Onyx::OverlayWindowFlag_NoCloseButton);
        ui->PopStyleVar();
        if (open)
        {
            ui->DockSpace(panels, Onyx::OverlayDockNodeFlag_CanBeEmpty, Onyx::OverlayWindowFlag_ChildGrow);
            ui->EndWindow();
        }
    }

    const GuideStep step = findGuideStep(app);
    // only while the visual review is on screen: the automated tests have nothing to do with any view
    const PaneId focus = app.ShowTests && app.ReviewTabOpen && step.Visual ? step.Visual->Pane : Pane_None;
    if (focus != app.FocusPane)
        app.FocusSince = app.Time;
    app.FocusPane = focus;

    drawDashboard(app, step);
    drawManual(app);
    drawTests(app, step);
    if (app.Mode == Mode_Docked)
        drawGalleryPanels(app);
    drawCategoryPanels(app);
    if (app.ShowOverlayDemo)
        ui->ShowDemo(&app.ShowOverlayDemo);

    if (app.PlacePanels > 0)
        --app.PlacePanels;
    if (app.VerdictsDirty)
    {
        saveVerdicts(app.Categories);
        app.VerdictsDirty = false;
    }
}

// =====================================================================================================================
//  11. CATEGORIES
// =====================================================================================================================
//
//  What each panel shows: its title, a one line summary, the concepts behind it (behind its help button), the
//  function that draws its panel, and its visual checks. A visual check says what to do in one or two lines, explains
//  more on hover, and has a priority for the visual review: DPI and rendering correctness come before cosmetics.
//  Everything a program can judge is an automated test in tests/suites instead.

static void setupCategories(App &app)
{
    Category &dpi = app.Categories[Category_Dpi];
    dpi.Title = "DPI and content scale";
    dpi.Summary = "Logical vs physical pixels, scale changes, swapchain extent";
    dpi.Concepts =
        "A physical pixel is a dot on the screen. A logical pixel is what your layout is written in. The content scale "
        "is the ratio between them: at 1.5x, a 100 x 100 logical rectangle covers 150 x 150 physical pixels. Onyx "
        "works "
        "in logical pixels everywhere (window size, viewports, mouse, overlay layout) and only multiplies by the "
        "content scale where real pixels are allocated: swapchain images, view framebuffers and render textures. A DPI "
        "bug is almost always a place where the two spaces get mixed: blurry output (rendering at logical resolution "
        "on "
        "a physical framebuffer), clipped output (physical sizes used as logical) or a drifting cursor (mouse in one "
        "space, viewport in the other).\n\nGLFW's screen coordinates are logical on Wayland and macOS, and physical on "
        "X11 and Windows. Onyx hides that: Window::GetSize() and GetMousePosition() always return logical pixels, "
        "derived from the framebuffer size divided by the content scale.\n\nOn Hyprland, change the scale with: "
        "hyprctl keyword monitor <name>,preferred,auto,1.5 (also try 1, 1.25 and 2).";
    dpi.Panel = panelDpi;
    dpi.Visuals.Append(VisualCheck{
        .Name = "Pixel grid is pixel perfect at integer scales",
        .Do = "At 1x and 2x, the 8 px grid and the 1 px checkerboard of the pixel view are perfectly crisp and evenly "
              "spaced.",
        .More =
            "At fractional scales (1.25x, 1.5x) one logical pixel covers a fractional number of physical pixels, so "
            "some lines look slightly thicker than others. That is expected. A blurry, smeared look, like an "
            "upscaled image, is not. The docked layout can put a panel's image at a fractional position, which "
            "softens it, so this is judged in the direct layout.",
        .Priority = 10,
        .Pane = Pane_Pixel,
        .Needs = Requires_DirectLayout});
    dpi.Visuals.Append(VisualCheck{
        .Name = "Cursor markers sit exactly under the mouse",
        .Do = "Move the mouse through every view: the green ring sits right under the cursor, also near the right and "
              "bottom edges.",
        .More = "Every view draws a green ring at the mouse position, the textures view included, whose viewport does "
                "not start at the window origin. Drift that grows towards the bottom right means logical and physical "
                "coordinates are being mixed. In the docked layout the ring also goes through the mapping from the "
                "panel's image into its texture.",
        .Priority = 11,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    dpi.Visuals.Append(VisualCheck{
        .Name = "Corner brackets are visible in every view",
        .Do = "Resize the window or change the scale: every view keeps tinted brackets in its four corners.",
        .More = "Missing brackets on the right or bottom mean the viewport or scissor is computed in the wrong pixel "
                "space. In the docked layout they also show that each texture follows its panel.",
        .Priority = 12,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    dpi.Visuals.Append(VisualCheck{
        .Name = "Views re-tile without gaps after a scale change",
        .Do = "Change the monitor scale: the four tinted views still cover the window exactly, with no black strip or "
              "overlap.",
        .More = "Each view has its own background tint. Their proportional viewports are recomputed from the logical "
                "window size after every resize and scale change.",
        .Priority = 13,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    dpi.Visuals.Append(VisualCheck{
        .Name = "Overlay text is sharp at every scale",
        .Do = "After a scale change, the text in these windows stays sharp, not just bigger or smaller.",
        .More = "The overlay renders through its own view at the window's content scale. Blurry text after a change "
                "means that view kept the old scale.",
        .Priority = 18,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});

    Category &views = app.Categories[Category_Views];
    views.Title = "Render views and cameras";
    views.Summary = "Viewports, coordinate conversions, camera modes, layers, the overlay's view, an inspector";
    views.Concepts =
        "A render target is something that can be rendered to: a window or a render texture. A render view is a "
        "rectangle (viewport) inside a target, with its own framebuffers, camera and passes. A camera decides what "
        "part "
        "of the world the view shows: CameraMode_Orthographic shows OrthoParameters.Size world units vertically, "
        "CameraMode_Viewport maps one world unit to one logical pixel, CameraMode_Perspective (3D only) uses a field "
        "of "
        "view. A render context records draws and can target several views at once.\n\nViewport, scissor and image "
        "size can each be proportional (fractions of the parent, recomputed on resize) or fixed (absolute logical "
        "pixels). Views are drawn in increasing Layer order: BringToTop and BringToBottom move a view to either end. "
        "Coordinates: screen = logical pixels from the target's top left, viewport = the same relative to the view, "
        "world = what the camera sees.\n\nThe overlay draws through its own 2D view covering the window, on top of "
        "every other view. In the docked layout every gallery's views live in its own render texture instead of a "
        "window quadrant, and a hidden view neither renders nor composites.";
    views.Panel = panelViews;
    views.Visuals.Append(VisualCheck{
        .Name = "3D gallery title, brackets and cursor are visible",
        .Do = "The 3D gallery shows its title, tinted corner brackets and the green ring under the mouse.",
        .More =
            "They are drawn by a transparent 2D view stacked on top of the 3D gallery. If they are missing, 2D views "
            "are composited below 3D views regardless of their layer. In the docked layout the same stacking "
            "happens inside a render texture.",
        .Priority = 16,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    views.Visuals.Append(VisualCheck{
        .Name = "Cameras only move in the hovered view",
        .Do = "Hover the 2D gallery and press W A S D Q E, then do the same over the 3D gallery. Only the hovered "
              "gallery moves.",
        .More = "The pixel view and the textures view never move. In the docked layout only the hovered panel's camera "
                "may move, and only while its image is the topmost thing under the mouse.",
        .Priority = 50,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    views.Visuals.Append(VisualCheck{
        .Name = "2D zoom is anchored at the cursor",
        .Do = "Scroll over the 2D gallery: the world point under the cursor stays under the cursor.",
        .More = "RenderView::ZoomScroll changes the ortho size and moves the camera so the point under the cursor does "
                "not move. In the docked layout the cursor goes through the panel's image mapping first.",
        .Priority = 51,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    views.Visuals.Append(VisualCheck{
        .Name = "Orthographic 3D camera keeps shading and shadows",
        .Do = "In the Render views panel set 3D camera mode to Orthographic: the 3D gallery loses its perspective "
              "(parallel edges stay parallel), but every shape stays lit and keeps its shadows. Scrolling zooms by "
              "changing the ortho size.",
        .More =
            "Near and far are view space depths around the camera and can be negative. The sun's shadow cascades "
            "are split between them, so they must cover the gallery: with a much larger range, cascades get coarse.",
        .Priority = 54,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    views.Visuals.Append(VisualCheck{
        .Name = "Overlay stays on top",
        .Do = "Drag these windows over every view, the 3D gallery included: they are always drawn above the scene.",
        .More = "Layers order views of both dimensions together. A 3D view drawn over the overlay means the compositor "
                "orders 2D and 3D views separately.",
        .Priority = 52,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    views.Visuals.Append(VisualCheck{
        .Name = "Tooltips stay inside the window",
        .Do = "Hover a test or a check near the right and the bottom edges of the window: its tooltip stays fully "
              "inside.",
        .More =
            "Without window promotion, always the case on Wayland, tooltips are drawn in the main view. The overlay "
            "shifts them left and up when they would cross the right or the bottom edge. They are kept narrow, "
            "so they fit as long as the window is wider than a tooltip.",
        .Priority = 95,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});

    Category &labCat = app.Categories[Category_Lab];
    labCat.Title = "View lab";
    labCat.Summary = "One more view to reconfigure: viewport, scissor, image scale, flags, camera, layer";
    labCat.Concepts =
        "The lab is a 2D view on the window, above the scene views and under the overlay, that you can reconfigure "
        "at will. Its viewport can be proportional (window fractions, recomputed on every resize) or fixed (logical "
        "pixels). Its scissor is a fraction of the viewport: only that part is drawn. Its image scale renders the view "
        "at a fraction (or a multiple) of its viewport size, and the result is stretched over the viewport.\n\nIts "
        "flags turn whole passes on and off: outlines (the outlined circle and square), transparency (the translucent "
        "square) and FXAA (watch the thin lines and the circle). Its camera is orthographic (Ortho size world units "
        "from top to bottom) or in viewport mode (one world unit per logical pixel), and can be moved and rotated. The "
        "grid lines are one world unit apart, or 50 pixels in viewport mode, and the axes cross at the world origin.";
    labCat.Panel = panelLab;
    labCat.Visuals.Append(VisualCheck{
        .Name = "The lab sits where its viewport says",
        .Do = "View lab panel: show the lab and drag its position and size, in both modes. Resizing the window moves "
              "a proportional lab, never a fixed one.",
        .More = "The purple brackets mark the corners of the lab's viewport. The view tests check the viewport math, "
                "this one checks that the view is drawn there.",
        .Priority = 55,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "The scissor clips the lab",
        .Do = "Shrink and move the scissor: the lab is drawn only inside its thin outline, the scene shows everywhere "
              "else, and corner brackets outside the scissor disappear.",
        .More = "The outline is drawn a pixel inside the scissor, so all four sides show exactly when the scissor is "
                "where its numbers say.",
        .Priority = 56,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "The scissor holds at any image scale",
        .Do = "Move the scissor to the right half, then set the image scale to 0.5 and to 2: the same area stays "
              "visible, and it is fully drawn.",
        .More = "Inside the view's passes the scissor is in image pixels, so it must be scaled by the image size over "
                "the viewport size, not only by the content scale. Otherwise a scaled image draws the wrong part, or "
                "nothing at all, inside the scissor.",
        .Priority = 57,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "Image scale changes the resolution only",
        .Do = "Drag the image scale: below 1 the lab gets blockier, above 1 smoother, but nothing in it moves or "
              "changes size.",
        .More = "The image is stretched over the viewport by the compositor, so world positions on screen do not "
                "depend on the image size.",
        .Priority = 58,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "Flags turn their passes on and off",
        .Do = "Untick Outlines: the circle and the square lose their outlines. Untick Transparency: the translucent "
              "square disappears. Nothing else changes.",
        .More = "Outlined draws need RenderViewFlag_Outlines, translucent ones need RenderViewFlag_Transparency. "
                "Toggling either recreates the view's framebuffers.",
        .Priority = 59,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "A transparent view stays transparent",
        .Do = "Tick Transparent background: the scene shows through the lab. Then tick FXAA: it still does.",
        .More = "The compositor blends every view over the ones below with its alpha. A pass that writes alpha 1 "
                "everywhere turns a transparent view opaque, and hides what is under it.",
        .Priority = 60,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "Layer buttons reorder the lab",
        .Do = "Press Below the scene: the lab disappears under the four views. Press Above the scene: it is back on "
              "top, still under these windows.",
        .More = "BringToTop and BringToBottom set a view's layer, and views are composited in increasing layer order. "
                "The overlay is brought back on top right after the lab.",
        .Priority = 61,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});
    labCat.Visuals.Append(VisualCheck{
        .Name = "Lab coordinates follow the mouse",
        .Do = "Move, rotate and zoom the lab's camera, then hover the lab: the ring sits under the cursor, and the "
              "world readout agrees with the grid and its labels.",
        .More = "Grid lines are one world unit apart (50 in viewport mode), the axes cross at the origin and the green "
                "cross sits at one step along both axes.",
        .Priority = 62,
        .Pane = Pane_None,
        .Needs = Requires_DirectLayout});

    Category &rendering = app.Categories[Category_Rendering];
    rendering.Title = "Rendering";
    rendering.Summary = "Primitives, render modes, outlines, blending, context state";
    rendering.Concepts =
        "Onyx draws five kinds of geometry: circles (analytic, with hollowness, arcs and fades), static meshes "
        "(triangle, quad, box...), parametric meshes (stadium, rounded rect, capsule, torus: one mesh, shape set per "
        "instance), glyphs (text) and dynamic meshes (your own vertices). Each draw is flat (unlit color), shaded (lit "
        "by the context's lights, using its material) and optionally outlined. Outlines are drawn in their own full "
        "screen pass, so the view needs RenderViewFlag_Outlines. RenderViewFlag_FXAA adds an anti-aliasing pass after "
        "it, with a quality set by RenderView::AntiAliasingMode.\n\nOpaque draws go first and "
        "write depth. Translucent draws (alpha below 1) go to the transparent pass and are blended on top, if the view "
        "has RenderViewFlag_Transparency.";
    rendering.Panel = panelRendering;
    rendering.Visuals.Append(VisualCheck{
        .Name = "Every 2D primitive is drawn",
        .Do = "2D gallery, top row: triangle, quad, circle (hollow, partial arc), stadium and rounded rect.",
        .More = "All five use the selected render mode. Turning outlines off must leave the same five shapes.",
        .Priority = 20,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(
        VisualCheck{.Name = "Every 3D primitive is drawn",
                    .Do = "3D gallery: box, sphere, cylinder, capsule, rounded box and torus over a floor.",
                    .More = "They rotate when Rotate shapes is on, and are shaded by the gallery's lights.",
                    .Priority = 21,
                    .Pane = Pane_Gallery3,
                    .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "Transparency blends in order",
        .Do = "2D gallery, middle: three overlapping translucent squares. Every overlap is tinted by both colors, and "
              "the alpha slider fades all three.",
        .More = "Translucent draws go to the transparent pass, which sorts and blends them. The view needs "
                "RenderViewFlag_Transparency.",
        .Priority = 22,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "3D transparency blends from any angle",
        .Do = "3D gallery, front right: three translucent shapes. Each shows what is behind it, other translucent "
              "shapes included, and the 3D alpha slider fades them. Look from several sides with WASD and LShift.",
        .More = "Translucent 3D draws go to the transparent pass, which blends them without sorting (weighted, order "
                "independent). Overlaps must look the same whatever the drawing order, with no popping as the camera "
                "moves. They keep their lighting and, with outlines on, their outlines.",
        .Priority = 28,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "Circle parameters are respected",
        .Do = "Drag the circle sliders of the Rendering panel: the circle follows hollowness, arc and fades exactly.",
        .More = "Circles are analytic: hollowness, the arc's angles and the inner and outer fades are evaluated per "
                "pixel. Fades need the blend pass, which the gallery turns on for them.",
        .Priority = 23,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "Text renders crisp",
        .Do = "The titles inside the scene views are crisp and correctly kerned, at any zoom.",
        .More = "Scene text uses the default font's glyphs, rendered from distance fields, so it must stay sharp when "
                "scaled.",
        .Priority = 24,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "Outlines are uniform",
        .Do = "With outlines on, every shape gets an even outline that follows its silhouette. The width slider "
              "changes all of them.",
        .More = "No gaps or spikes at corners. Outlines are drawn in their own full screen pass, so they need "
                "RenderViewFlag_Outlines.",
        .Priority = 80,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "FXAA smooths edges",
        .Do = "Rendering panel: turn outlines off and FXAA on. The edges of the 2D shapes and the 3D silhouettes get "
              "smoother, more so in the quality modes than in Fast.",
        .More = "FXAA finds edges by their contrast and blends across them. Text and the 1 px lines get slightly "
                "softer, which is expected. No change at all means the pass is not running, or its result is not the "
                "one composited. Toggling FXAA recreates the view's framebuffers.",
        .Priority = 25,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "FXAA leaves the view borders alone",
        .Do = "With FXAA on, the borders of both galleries get no light rim, even where the background is dark.",
        .More = "FXAA samples the neighbours of every pixel. At the border of a view they fall outside the image, "
                "and must repeat the edge pixel. A sampler with a white border color bleeds white into every edge.",
        .Priority = 26,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    rendering.Visuals.Append(VisualCheck{
        .Name = "Nothing flickers with an uncapped frame rate",
        .Do = "Platform panel: set the present mode to Immediate. Toggle FXAA and outlines: no view flickers, shows "
              "an older frame or goes black for a frame.",
        .More = "Every view keeps a few framebuffers, so a frame can render while the previous one is on the GPU. "
                "Capped by vsync, a view almost always gets its first one, and a bug in the others shows up once in "
                "a while. Uncapped, frames overlap and all of them are used: the Render views inspector counts them.",
        .Priority = 27,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});

    Category &lights = app.Categories[Category_Lights];
    lights.Title = "Lights and shadows";
    lights.Summary = "2D point, cone and directional lights, occluders, 3D point and spot lights, shadow filters";
    lights.Concepts =
        "Lights only affect shaded draws, through their material. In 2D, occluder materials block light: for every "
        "shadow casting light and view, the shadow pass draws the shaded occluders into an occlusion map around the "
        "light, and a compute pass ray marches it into a 1D shadow map. Initialize sets how many of each there are per "
        "frame (MaxShadowMaps, MaxOcclusionMaps), shared by every 2D light and view: two by default. A 2D "
        "point light can be a cone (Angle, Extent and Decay), and a 2D directional light "
        "is a band of light along an angle.\n\nIn 3D every shaded mesh casts and receives: directional lights use "
        "cascades bound to a view (asserted), point lights a cube map up to ShadowRadius and spot lights a perspective "
        "map up to ShadowRange. 3D point and spot shadows fade over the last 10% of that range. Light positions, "
        "directions and radii are in world space: the context transform does not apply to lights.\n\nShadow filtering "
        "is per light: hard by default, LightFlag_PCF for smooth edges, LightFlag_PCSS for soft shadows that widen "
        "with LightSize (point and spot lights) or AngleSize (directional lights). PCSS wins when both are set.";
    lights.Panel = panelLights;
    lights.Visuals.Append(VisualCheck{
        .Name = "2D light follows the cursor and casts shadows",
        .Do = "Hover the 2D gallery: a warm light, ringed in yellow, follows the cursor and lights the dark band. The "
              "occluders cast shadows away from it.",
        .More = "With the mouse elsewhere the light wanders between the occluders. Its shadows reach ShadowRadius (8 "
                "units). Unlike 3D, the 2D point light does not fade there: its occlusion map only covers that radius.",
        .Priority = 30,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "No light leaks through 2D occluders",
        .Do = "Turn on Sweep in the Lights panel and watch the spinning quad: no slit of light crosses it or its "
              "shadow, at any angle.",
        .More =
            "The sweep orbits the light around the quad. When the light sits to the quad's right, the quad is at 180 "
            "degrees from it, where the circular shadow map wraps around: that is where a seam leaked light.",
        .Priority = 31,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Static meshes cast 2D shadows",
        .Do = "The spinning quad and the triangle in the band block the light: their shadows have their shape.",
        .More = "Each occluder is a shaded draw with an occluder material, and the 2D shadow pass draws every kind of "
                "geometry. One that does not cast is a library bug.",
        .Priority = 32,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Circles cast 2D shadows",
        .Do = "The circle in the band blocks the light and casts a round shadow.",
        .More = "Circles are analytic: the shadow pass evaluates the same circle per pixel, hollowness and arcs "
                "included.",
        .Priority = 33,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Parametric meshes cast 2D shadows",
        .Do = "The stadium and the rounded rect in the band block the light: their shadows follow their rounded "
              "shapes.",
        .More = "Parametric meshes cut their shape per pixel, in the shadow pass too.",
        .Priority = 34,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Text casts 2D shadows",
        .Do = "The ONYX text in the band blocks the light letter by letter.",
        .More = "Glyphs are drawn from distance fields: the shadow pass must sample them too, or the text would cast "
                "solid boxes, or nothing.",
        .Priority = 35,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "2D directional light casts shadows away from it",
        .Do =
            "With the directional light on, every occluder casts a second, bluish shadow in the direction of the blue "
            "arrow. Rotate it with Direction.",
        .More = "The 2D directional light is a band of light along its direction, in world space. The same occluders "
                "must block both lights, and its shadows run from them in the direction it travels.",
        .Priority = 36,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "2D cone light has clean edges",
        .Do = "Turn on Cone: the cursor light becomes a cone aimed where it moves. Its edges are straight, softened by "
              "Edge softness.",
        .More = "Extent is half of the cone as a fraction of half a turn, and Decay fades the light from the axis to "
                "the edges. Shadows inside the cone stay correct, and nothing behind the light is lit by it.",
        .Priority = 37,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "PCF smooths shadow edges",
        .Do = "Switch 2D shadow filter between Hard and PCF in the Lights panel: hard shadow edges are crisp and turn "
              "jagged far from the light, PCF blends them into a thin gradient. The 3D shadow filter does the same on "
              "the floor shadows.",
        .More = "PCF averages a few shadow map samples around each lookup, a fixed number of texels wide: the light's "
                "size does not change it.",
        .Priority = 38,
        .Pane = Pane_Gallery2,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "3D point light is visible and its shadows are complete",
        .Do = "A small yellow sphere marks the orbiting point light. Its shadows stay solid wherever it is.",
        .More = "No hole may open from the middle of a shadow as the light moves away. Shadows only fade near "
                "ShadowRadius (14 units here), over its last 10%.",
        .Priority = 40,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "3D shadows are correct",
        .Do = "Every 3D shape casts a shadow on the floor, consistent with the light and following the shape as it "
              "rotates.",
        .More = "No shadow acne (striped self shadowing on lit faces), no shadows cut off at cascade boundaries while "
                "moving the camera. The directional light's cascades follow the active 3D view.",
        .Priority = 41,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Spot cone matches the lit area on the floor",
        .Do = "Turn on Spot only. The blue lines from the spot outline its cone: the lit area on the floor fills that "
              "outline exactly.",
        .More = "The outline is computed from the spot's position, direction and field of view, independently of the "
                "renderer. Change the field of view, tilt and turn: the light must follow the outline.",
        .Priority = 42,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Spot shadow of the shape is complete",
        .Do = "The white box under the spot casts a solid shadow inside the lit area, without holes or gaps.",
        .More = "The spot's shadow map reaches ShadowRange (20 units) and shadows fade over its last 10%. The box is "
                "well inside it.",
        .Priority = 43,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Nothing outside the cone is lit by the spot",
        .Do = "With Spot only, the floor and the shapes outside the outline get no spot light, only the ambient one.",
        .More = "The spot's edge is sharp unless its Decay softens it. LightRange only sets how fast it falls off with "
                "distance, it does not cut it.",
        .Priority = 44,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Sun pointing straight down still lights",
        .Do = "Untick Point light, Spot light and Spot only, then tick Sun straight down: the tops of the shapes stay "
              "lit and their shadows fall right under them.",
        .More = "A light's frame is built from its direction and a helper vector. A helper parallel to the direction "
                "gives a zero cross product and a NaN frame, and the light goes dark. The test "
                "math.orthogonal_bases_handle_vertical_directions covers the helper used for directional lights.",
        .Priority = 45,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Spot pointing straight down keeps its shadow",
        .Do = "Tick Spot light and Spot only, then press Spot straight down: the box still casts a solid shadow inside "
              "the lit area.",
        .More = "The spot's shadow map is rendered from a view looking along its direction. Built with a helper vector "
                "parallel to that direction, the view comes out NaN: the shadow vanishes, or the whole lit area goes "
                "dark, depending on the GPU.",
        .Priority = 46,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});
    lights.Visuals.Append(VisualCheck{
        .Name = "Soft shadows harden near the occluder",
        .Do = "Pick Soft (PCSS) as the 3D shadow filter: shadows are sharp where they leave the shape that casts them "
              "and blur farther away. Raise Point light size, Spot light size or Sun angular size and the blur grows. "
              "2D shadows do the same with 2D light size.",
        .More = "PCSS first searches the shadow map for blockers, then widens the filter with the distance between "
                "blocker and receiver times the light's size. Where a shape touches what it shadows, that distance is "
                "zero and the edge stays sharp.",
        .Priority = 47,
        .Pane = Pane_Gallery3,
        .Needs = Requires_Nothing});

    Category &textures = app.Categories[Category_Textures];
    textures.Title = "Render textures";
    textures.Summary = "Offscreen targets, their content scale, sampling and docked galleries";
    textures.Concepts =
        "A render texture is an offscreen render target. You create views on it like on a window, render into them "
        "with contexts, and then sample the result as a texture: in a context (ctx->Image) or in the overlay "
        "(ui->Image). It has its own logical size and content scale. Rendering at the window's scale gives a texture "
        "that maps one texel to one physical pixel when drawn at its logical size.\n\nIn the docked layout every "
        "gallery "
        "renders into its own render texture, which the demo resizes to its panel with RenderTexture::Resize.";
    textures.Panel = panelTextures;
    textures.Visuals.Append(VisualCheck{
        .Name = "Scaled render texture stays sharp",
        .Do = "Textures view and the Render textures panel: the left texture looks as crisp as the pixel view, the "
              "right one softer above 1x.",
        .More = "The left render texture follows the window content scale, the right one is fixed at scale 1. At 1x "
                "both must look identical.",
        .Priority = 14,
        .Pane = Pane_Textures,
        .Needs = Requires_Nothing});
    textures.Visuals.Append(VisualCheck{
        .Name = "Docked galleries stay crisp when panels resize",
        .Do = "Drag the dock splits: every gallery redraws at its panel's size, never stretched or blurry once it "
              "settles.",
        .More = "Each docked gallery renders into a render texture resized to its panel, at the window's content "
                "scale, one frame after the panel changes.",
        .Priority = 15,
        .Pane = Pane_None,
        .Needs = Requires_DockedLayout});
    textures.Visuals.Append(VisualCheck{
        .Name = "A panel docked on the parent fills the window",
        .Do = "Drag the 3D gallery's tab and drop it in the middle of the whole window's docking guides: it fills the "
              "window as a tab next to All panels, and the gallery redraws at that size.",
        .More = "The docked layout nests a parent dockspace, holding All panels, which holds every panel. Docking on "
                "the parent gives a panel the whole window. Switching to the direct layout and back must put every "
                "panel in place again.",
        .Priority = 17,
        .Pane = Pane_Gallery3,
        .Needs = Requires_DockedLayout});

    Category &input = app.Categories[Category_Input];
    input.Title = "Input and events";
    input.Summary = "Event stream consistency, keyboard, mouse, text input";
    input.Concepts =
        "Each window collects events (keys, mouse, scroll, resize, focus, scale...) while Onyx polls the platform. "
        "Window::GetNewEvents() returns the events gathered since the previous frame. Polling (IsKeyPressed, "
        "GetMousePosition) returns the state right now. Both views of the input must agree, and the event stream must "
        "be internally consistent: a release always follows a press, focus gain and loss alternate, and so on. The "
        "overlay consumes input too: WantCaptureMouse, WantCaptureScroll and WantCaptureKeyboard tell the application "
        "when to ignore it.";
    input.Panel = panelInput;
    input.Visuals.Append(VisualCheck{
        .Name = "Overlay captures the mouse",
        .Do = "Drag an overlay window over a gallery and scroll inside a panel: the cameras neither move nor zoom.",
        .More = "In the direct layout the demo gates camera control on WantCaptureMouse and WantCaptureKeyboard, and "
                "zoom on WantCaptureScroll. Docked galleries live inside overlay windows, so they use their image's "
                "hover instead.",
        .Priority = 53,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});
    input.Visuals.Append(VisualCheck{
        .Name = "Typing reaches the event stream",
        .Do = "Type a few letters with no text field focused: the Typed text line of the Input panel shows them, in "
              "order.",
        .More = "Every printable Event_CharInput is appended to that line. Missing or reordered characters mean the "
                "event stream drops or reorders text input.",
        .Priority = 60,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});

    Category &audio = app.Categories[Category_Audio];
    audio.Title = "Audio";
    audio.Summary = "Procedural sources, playback state, end callbacks, spatialization";
    audio.Concepts =
        "Onyx's audio wraps miniaudio and is optional: it compiles only with ONYX_ENABLE_AUDIO. A source is a "
        "resource: a decoded buffer, a stream, or, as here, a callback that fills interleaved frames on the audio "
        "thread and returns how many it wrote. Returning 0 ends the sound. The callback must not allocate or lock, so "
        "this demo passes its parameters through atomics, and gives every source a seek callback: miniaudio loops and "
        "replays a source by seeking it back to frame 0.\n\nVolume, pitch, pan and looping are per source, the master "
        "volume is global. End callbacks run on the audio thread, or are queued and run later on the main thread. "
        "Spatialized sources (AudioFlag_Spatialization) are placed in world units around a listener.";
    audio.Panel = panelAudio;
#ifdef ONYX_ENABLE_AUDIO
    audio.Visuals.Append(VisualCheck{
        .Name = "The sound is audible",
        .Do = "Audio panel: press Play. You hear the selected wave for about a second, or until Stop with Loop on.",
        .More = "Every sound is generated in code. The scope shows what the callback produced, so it moves even with "
                "the volume down.",
        .Priority = 70,
        .Pane = Pane_None,
        .Needs = Requires_Audio});
    audio.Visuals.Append(
        VisualCheck{.Name = "The pitch slider changes pitch",
                    .Do = "Loop a sine and drag Pitch: the tone goes up and down smoothly, without clicks or gaps.",
                    .More = "Pitch resamples the source: 2 is an octave up, 0.5 an octave down.",
                    .Priority = 71,
                    .Pane = Pane_None,
                    .Needs = Requires_Audio});
    audio.Visuals.Append(
        VisualCheck{.Name = "The pan slider moves the sound between channels",
                    .Do = "Loop a tone and drag Pan from -1 to 1: it moves from the left speaker to the right one.",
                    .More = "Pan applies to sources without spatialization. Headphones make it obvious.",
                    .Priority = 72,
                    .Pane = Pane_None,
                    .Needs = Requires_Audio});
    audio.Visuals.Append(VisualCheck{
        .Name = "The spatial source moves around you",
        .Do = "Start the 2D orbit, then the 3D orbit: the beeps circle you, left and right, louder when near.",
        .More = "In 2D the source circles the listener at the 2D gallery's center. In 3D the listener is the 3D "
                "camera: move it and the beeps keep circling it.",
        .Priority = 73,
        .Pane = Pane_None,
        .Needs = Requires_Audio});
#endif

    Category &resources = app.Categories[Category_Resources];
    resources.Title = "Resources";
    resources.Summary = "Default resources, materials, fonts, pools, frame usage against the specs";
    resources.Concepts =
        "Meshes, fonts, materials, samplers, images and textures are referred to by handles (Onyx::Resource). Unlike "
        "raw Graphite handles, they are bound and ready to use: give a handle to a context or a material and it just "
        "works. Meshes and fonts are grouped in pools. Registering a resource only records it: Resources_Sync uploads "
        "everything pending to the GPU, so sync after a batch of registrations.\n\nOnyx::Initialize sizes the "
        "renderer's per frame buffers from Onyx::Specs: instance buffers per geometry, lights, shadow maps, dynamic "
        "vertices, draw commands. Each is created once per resource group, so their cost multiplies by "
        "ResourceGroupCount. Overflowing one asserts.";
    resources.Panel = panelResources;

    Category &platform = app.Categories[Category_Platform];
    platform.Title = "Platform and device";
    platform.Summary = "Monitors, window properties, swapchain, GPU";
    platform.Concepts =
        "Graphite (Graph) is the layer between Onyx and the system: a platform backend (GLFW) for windows, monitors "
        "and input, and a render backend (Vulkan) for devices, swapchains, pipelines and resources. Onyx windows wrap "
        "a Graphite window and own its swapchain. Changing the present mode or the size recreates the swapchain.\n\n"
        "Frame pacing is per window: with a target delta time (SetTargetDeltaTime), Render skips a window until it "
        "is due and sleeps until the next one is. A window opened in immediate mode targets the monitor's refresh "
        "rate unless OpenWindowFlag_NoDefaultDeltaTime is given. In vsync, a swapchain recreation or a window move "
        "resets the target to the monitor's.";
    platform.Panel = panelPlatform;
    platform.Visuals.Append(VisualCheck{
        .Name = "Frame pacing follows the target",
        .Do = "In the Platform panel pick Immediate, then Custom rate at 30 Hz: the dashboard reads about 30 fps and "
              "animations keep their speed. Monitor refresh rate brings it to the monitor's rate, Present mode only "
              "lets immediate run as fast as it can.",
        .More = "Animations advance by the window's delta time, so they must look the same at any rate, only less "
                "smooth. In vsync a target above the monitor's rate changes nothing.",
        .Priority = 90,
        .Pane = Pane_None,
        .Needs = Requires_Nothing});

    app.LoadedVerdicts = loadVerdicts(app.Categories);
}

// =====================================================================================================================
//  12. MAIN LOOP
// =====================================================================================================================

int main()
{
    // the defaults cover most of this demo: only what differs is asked for, and the Resources panel measures every
    // maximum against what a frame uses. the per dimension maxima size real buffers once per resource group, while
    // pool caps such as MaxImages only reserve handles
    Onyx::Initialize({.Platform = DemoPlatform,
                      .MaxImages = 512, // framebuffers of 14 views (up to 18 images each), textures, shadow maps
                      .ListenerCount = 1,
                      // every visible glyph of the overlay is a 2D instance, and they peak with every panel open
                      .Specs2 = {.MaxInstances = 32000},
                      // a few dozen shapes, and no 3D dynamic meshes at all. the 3D gallery targets one view at a time,
                      // so the default single shadow map per light type is enough
                      .Specs3 = {.MaxInstances = 1024,
                                 .MaxDrawCommands = 1024,
                                 .MaxDynamicVertices = 0,
                                 .MaxDynamicIndices = 0,
                                 .MaxDynamicMeshes = 0}});
    Onyx::Resources_CreateDefault();

    App app{};
    setup(app);
    setLayout(app, Mode_Direct);
    setupCategories(app);
#ifdef ONYX_ENABLE_AUDIO
    setupAudio(app);
#endif
    listTests(app.Tests);

    while (Onyx::Running())
    {
        app.DeltaTime = Onyx::GetDeltaTime(app.Window).AsSeconds();
        app.Time += app.DeltaTime;
        app.FrameTimes[app.FrameTimeHead] = app.DeltaTime;
        app.FrameTimeHead = (app.FrameTimeHead + 1) % app.FrameTimes.GetSize();
        trackFramebuffers(app);

        // a layout change asked for last frame applies before anything is drawn, so the whole frame is consistent
        // the overlay's flags can change at runtime (the overlay demo has checkboxes for them). without docking, the
        // docked layout cannot hold, so it falls back to the direct one
        if (app.RequestedMode == Mode_Docked && !(app.Ui->Flags & Onyx::OverlayFlag_Docking))
            app.RequestedMode = Mode_Direct;
        if (app.RequestedMode != app.Mode)
            setLayout(app, app.RequestedMode);
        updatePanes(app);
        processEvents(app);
        controlCameras(app);
        updateLab(app);
        updatePresentCycle(app);
#ifdef ONYX_ENABLE_AUDIO
        updateAudio(app);
#endif

        updateTests(app);
        drawScene(app);
        drawOverlay(app);
        app.Ui->Draw();
        measureUsage(app);
        updatePacing(app);

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
