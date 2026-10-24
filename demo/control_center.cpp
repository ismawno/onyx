// =====================================================================================================================
//  ONYX CONTROL CENTER
// =====================================================================================================================
//
//  A single executable that checks, in one place, that the nominal behaviour of Onyx (and the parts of Graphite and
//  Toolkit it stands on) holds. Run it, play with it, change the monitor scale, resize the window, move cameras around,
//  and watch the checks.
//
//  How to read the UI
//    PASS (green)   an automatic check that holds this frame
//    FAIL (red)     an automatic check that does not hold. The failure log in the "Control center" window keeps it,
//                   even if it only failed for a single frame
//    TODO (yellow)  a visual check only you can judge. Look at what it says and mark it "Looks right" or "Broken"
//    GOOD / BAD     a visual check you already judged
//    INFO (blue)    a measured value with no right or wrong answer
//
//  Scene layout: the window is split in four render views
//    top left      Pixel view    CameraMode_Viewport, 1 world unit = 1 logical pixel. DPI and rasterization
//    top right     2D gallery    orthographic camera. Every 2D primitive, outlines, blending and 2D lights
//    bottom left   3D gallery    perspective camera. Every 3D primitive, shading, shadows, point and spot lights
//    bottom right  Textures      render textures drawn in the scene, plus an offset viewport for the mouse checks
//
//  Controls
//    W A S D Q E          move/rotate the camera of the view under the mouse (2D gallery and 3D gallery only)
//    Space, LCtrl, LShift 3D only: up, down, toggle look around
//    Mouse wheel          zoom the view under the mouse (2D gallery: around the cursor, 3D gallery: field of view)
//
//  DPI testing tip (Hyprland): hyprctl keyword monitor <name>,preferred,auto,1.5   (also try 1, 1.25, 2)
//
//  File sections
//     1. Configuration and theme
//     2. Check bookkeeping: automatic checks, visual checks and the failure log
//     3. UI helpers: badges, colored text, explanations
//     4. Application state
//     5. Setup: resources, window, views, contexts, render textures, overlay
//     6. Event processing and camera control
//     7. Automatic checks, one function per category
//     8. Scene drawing, one function per view
//     9. Overlay panels, one function per category, plus the main "Control center" dashboard
//    10. Main loop
//
// =====================================================================================================================

#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"
#include "onyx/sanitizer_options.hpp"
#include "graph/platform.hpp"
#include "graph/swap_chain.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/profiling/clock.hpp"
#include <cmath>

using namespace TKit::Alias;
using Onyx::D2;
using Onyx::D3;
namespace Math = TKit::Math;

namespace
{
// =====================================================================================================================
//  1. CONFIGURATION AND THEME
// =====================================================================================================================

// force a platform backend here if you need to (Graph::Platform_Wayland, Graph::Platform_X11). it matters for DPI work,
// because each backend reports screen coordinates differently
constexpr Graph::Platform DemoPlatform = Graph::Platform_Auto;

constexpr u32v2 InitialWindowSize{1600, 900};
constexpr u32v2 RenderTextureSize{160, 160};

// how long the swapchain may lag behind a window size or scale change before it counts as a failure. swapchains are
// recreated lazily, so a mismatch that lasts a couple of frames is fine. one that sticks around is not
constexpr f32 SwapchainGracePeriod = 1.f;

constexpr f32 Epsilon = 1e-4f;

// proportional viewports of the four scene views (position, size), in window fractions
struct Quadrant
{
    const char *Name;
    f32 X, Y;
};
constexpr Quadrant Quadrants[4] = {
    {"Pixel view", 0.f, 0.f},
    {"2D gallery", 0.5f, 0.f},
    {"3D gallery", 0.f, 0.5f},
    {"Textures", 0.5f, 0.5f},
};
constexpr f32 QuadrantSize = 0.5f;

// check names must be unique string literals: the failure log tracks checks by name
constexpr const char *ViewportCheckNames[4] = {"Pixel view: proportional viewport", "2D gallery: proportional viewport",
                                               "3D gallery: proportional viewport", "Textures: proportional viewport"};
constexpr const char *ImageCheckNames[4] = {
    "Pixel view: image size follows viewport", "2D gallery: image size follows viewport",
    "3D gallery: image size follows viewport", "Textures: image size follows viewport"};

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

// each view gets a slightly different clear color, so they are easy to tell apart and any gap between them shows
const Onyx::Color ClearPixel{16u, 18u, 28u};
const Onyx::Color ClearGallery{20u, 24u, 36u};
const Onyx::Color ClearScene3{26u, 22u, 34u};
const Onyx::Color ClearTextures{18u, 28u, 28u};
const Onyx::Color ClearRenderTexture{40u, 34u, 60u};

const Onyx::Color Grid{48u, 54u, 76u};
const Onyx::Color Cursor{120u, 255u, 160u};
const Onyx::Color Corner{255u, 140u, 80u};

const Onyx::Color Shapes[6] = {
    Onyx::Color_Coral, Onyx::Color_Honey, Onyx::Color_Mint, Onyx::Color_Sky, Onyx::Color_Lavender, Onyx::Color_Peach,
};

// frame time bars go from green (on target) to red (twice the target or worse)
const Onyx::Color FrameTimeStops[3] = {Pass, Todo, Fail};
} // namespace Theme

// =====================================================================================================================
//  2. CHECK BOOKKEEPING
// =====================================================================================================================
//
//  Two kinds of checks live here:
//    - Automatic checks (AutoCheck): evaluated every frame, whether their panel is open or not, so the dashboard and
//    the
//      failure log always know the truth. Each one carries a rule (what must hold, in plain words) and a detail (what
//      was measured this frame)
//    - Visual checks (VisualCheck): things a program cannot judge (is it crisp? does it blend right?). They carry a
//      description of what to look at, and you give the verdict
//
//  A Category groups both kinds, and maps to one overlay panel.

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

template <typename... Args> Line Format(const fmt::format_string<Args...> str, Args &&...args)
{
    Line line;
    const auto result = fmt::format_to_n(line.Data, sizeof(line.Data) - 1, str, std::forward<Args>(args)...);
    line.Size = u32(result.size < sizeof(line.Data) - 1 ? result.size : sizeof(line.Data) - 1);
    line.Data[line.Size] = '\0';
    return line;
}

struct AutoCheck
{
    const char *Name;
    const char *Rule;
    Line Detail;
    bool Passed;
};

enum Verdict : u8
{
    Verdict_Unseen,
    Verdict_Good,
    Verdict_Broken,
};

struct VisualCheck
{
    const char *Name;
    const char *WhatToLookAt;
    Verdict Verdict = Verdict_Unseen;
};

struct FailureRecord
{
    f32 Time;
    const char *Category;
    const char *Name;
    Line Detail;
};

enum CategoryId : u8
{
    Category_Dpi,
    Category_Views,
    Category_Rendering,
    Category_Textures,
    Category_Input,
    Category_Resources,
    Category_Toolkit,
    Category_Platform,
    Category_Overlay,
    Category_Count,
};

struct Category
{
    const char *Title = "";
    const char *Summary = "";
    bool Open = false;

    TKit::StaticArray<AutoCheck, 32> Checks{};
    TKit::StaticArray<VisualCheck, 16> Visuals{};

    // names that failed last frame. a check is logged only when it goes from passing to failing, so a check that stays
    // red does not flood the log
    TKit::StaticArray<const char *, 32> FailingLastFrame{};
    TKit::StaticArray<u32, 32> NewFailures{};
    u32 FailuresSinceStart = 0;

    void BeginFrame()
    {
        FailingLastFrame.Clear();
        for (const AutoCheck &check : Checks)
            if (!check.Passed)
                FailingLastFrame.Append(check.Name);
        Checks.Clear();
        NewFailures.Clear();
    }

    // the heart of every automatic check: name, condition, rule and a formatted description of what was measured
    template <typename... Args>
    bool Expect(const char *name, const bool passed, const char *rule, const fmt::format_string<Args...> detail,
                Args &&...args)
    {
        TKIT_ASSERT(!Checks.IsFull(), "[CONTROL CENTER] Too many checks in category '{}'", Title);
        Checks.Append(AutoCheck{name, rule, Format(detail, std::forward<Args>(args)...), passed});
        if (!passed && !FailingLastFrame.Contains(name))
        {
            ++FailuresSinceStart;
            NewFailures.Append(Checks.GetSize() - 1);
        }
        return passed;
    }

    void AddVisual(const char *name, const char *whatToLookAt)
    {
        Visuals.Append(VisualCheck{name, whatToLookAt});
    }

    u32 CountPassed() const
    {
        u32 count = 0;
        for (const AutoCheck &check : Checks)
            count += check.Passed;
        return count;
    }
    u32 CountVisuals(const Verdict verdict) const
    {
        u32 count = 0;
        for (const VisualCheck &check : Visuals)
            count += check.Verdict == verdict;
        return count;
    }
};

// =====================================================================================================================
//  3. UI HELPERS
// =====================================================================================================================

void badge(Onyx::Overlay *ui, const char *text, const Onyx::Color &color)
{
    ui->BeginPanel({.FillColor = color,
                    .Alignment = Onyx::Overlay::Center,
                    .Sizing = {Onyx::LayoutSizing::Fit(48.f), Onyx::LayoutSizing::Fit()},
                    .Shape = Onyx::LayoutShape::Rectangle(4.f),
                    .Padding = f32v4{5.f, 5.f, 1.f, 1.f}});
    ui->PushStyleColor(Onyx::OverlayColor_Text, Theme::BadgeText);
    ui->TextRaw(text);
    ui->PopStyleColor();
    ui->EndPanel();
}

void coloredRaw(Onyx::Overlay *ui, const Onyx::Color &color, const TKit::StringView text,
                const Onyx::LayoutTextMode mode = Onyx::TextMode_Unbounded)
{
    ui->PushStyleColor(Onyx::OverlayColor_Text, color);
    ui->TextRaw(mode, text);
    ui->PopStyleColor();
}

template <typename... Args>
void colored(Onyx::Overlay *ui, const Onyx::Color &color, const fmt::format_string<Args...> str, Args &&...args)
{
    ui->PushStyleColor(Onyx::OverlayColor_Text, color);
    ui->Text(str, std::forward<Args>(args)...);
    ui->PopStyleColor();
}

// a dim paragraph that wraps to the window width
void explain(Onyx::Overlay *ui, const char *text)
{
    coloredRaw(ui, Theme::Dim, text, Onyx::TextMode_Wrapped);
}

// a collapsible block with background knowledge. every panel opens with one
void concepts(Onyx::Overlay *ui, const char *title, const char *text)
{
    if (ui->PushTree(title, Onyx::OverlayTreeFlag_Framed | Onyx::OverlayTreeFlag_StartOpen))
    {
        explain(ui, text);
        ui->PopTree();
    }
}

// a row of: badge + label + value
template <typename... Args>
void info(Onyx::Overlay *ui, const char *label, const fmt::format_string<Args...> str, Args &&...args)
{
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    badge(ui, "INFO", Theme::Info);
    coloredRaw(ui, Theme::Text, label);
    colored(ui, Theme::Info, str, std::forward<Args>(args)...);
    ui->PopPanel();
}

void drawAutoChecks(Onyx::Overlay *ui, const Category &cat, const bool showRules)
{
    ui->HorizontalSeparator("Automatic checks");
    for (const AutoCheck &check : cat.Checks)
    {
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        badge(ui, check.Passed ? "PASS" : "FAIL", check.Passed ? Theme::Pass : Theme::Fail);
        coloredRaw(ui, Theme::Text, check.Name);
        ui->PopPanel();
        coloredRaw(ui, check.Passed ? Theme::Info : Theme::Fail, check.Detail.View(), Onyx::TextMode_Wrapped);
        if (showRules)
            explain(ui, check.Rule);
    }
}

void drawVisualChecks(Onyx::Overlay *ui, Category &cat, const u32 idOffset)
{
    if (cat.Visuals.IsEmpty())
        return;
    ui->HorizontalSeparator("Visual checks (you are the judge)");
    for (u32 i = 0; i < cat.Visuals.GetSize(); ++i)
    {
        VisualCheck &check = cat.Visuals[i];
        ui->PushId(Onyx::LayoutId{usz(idOffset + i + 1)});

        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        if (check.Verdict == Verdict_Good)
            badge(ui, "GOOD", Theme::Pass);
        else if (check.Verdict == Verdict_Broken)
            badge(ui, "BAD", Theme::Fail);
        else
            badge(ui, "TODO", Theme::Todo);
        coloredRaw(ui, Theme::Text, check.Name);
        ui->PopPanel();

        explain(ui, check.WhatToLookAt);

        ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
        ui->RadioButton("Not checked", &check.Verdict, Verdict_Unseen);
        ui->RadioButton("Looks right", &check.Verdict, Verdict_Good);
        ui->RadioButton("Broken", &check.Verdict, Verdict_Broken);
        ui->PopPanel();

        ui->PopId();
    }
}

const char *platformName(const Graph::Platform platform)
{
    switch (platform)
    {
    case Graph::Platform_Win32:
        return "Win32";
    case Graph::Platform_Cocoa:
        return "Cocoa";
    case Graph::Platform_Wayland:
        return "Wayland";
    case Graph::Platform_X11:
        return "X11";
    default:
        return "Any";
    }
}

constexpr const char *EventNames[] = {
    "KeyPressed",     "KeyReleased",        "KeyRepeat",           "MouseMoved",   "MousePressed",
    "MouseReleased",  "MouseEntered",       "MouseLeft",           "Scrolled",     "WindowMoved",
    "WindowResized",  "WindowFocused",      "WindowUnfocused",     "WindowClosed", "WindowMinimized",
    "WindowRestored", "FramebufferResized", "ContentScaleChanged", "CharInput",    "SwapchainRecreated",
};
static_assert(sizeof(EventNames) / sizeof(EventNames[0]) == Onyx::Event_Count,
              "EventNames must list every Onyx::EventType, in order");

bool near(const f32 a, const f32 b, const f32 tolerance)
{
    return Math::Absolute(a - b) <= tolerance;
}
bool near(const f32v2 &a, const f32v2 &b, const f32 tolerance)
{
    return near(a[0], b[0], tolerance) && near(a[1], b[1], tolerance);
}
bool nearPixels(const u32v2 &a, const u32v2 &b, const i32 tolerance)
{
    return Math::Absolute(i32(a[0]) - i32(b[0])) <= tolerance && Math::Absolute(i32(a[1]) - i32(b[1])) <= tolerance;
}

// =====================================================================================================================
//  4. APPLICATION STATE
// =====================================================================================================================

struct RenderSettings
{
    bool Outlines = true;
    bool Rotate = true;
    bool Lights2 = true;
    bool Lights3 = true;
    bool PointLight3 = true;
    bool SpotLight3 = true;
    bool Shadows = true;
    bool PostProcess = true;
    f32 OutlineWidth = 0.08f;
    f32 Alpha = 0.6f;
    Onyx::CircleParameters Circle{.Hollowness = 0.3f, .UpperAngle = 1.6f * Math::Pi<f32>()};
};

struct InputTracking
{
    TKit::FixedArray<u32, Onyx::Event_Count> EventCounts{};

    // keys and buttons currently held according to the event stream (not according to polling)
    TKit::StaticArray<Graph::Key, 32> HeldKeys{};
    TKit::StaticArray<Graph::Mouse, 8> HeldButtons{};

    // invariant violations, accumulated since start
    u32 RepeatWithoutPress = 0;
    u32 ReleaseWithoutPress = 0;
    u32 FocusAlternation = 0;
    u32 EnterAlternation = 0;
    u32 MinimizeAlternation = 0;

    Onyx::EventType LastFocus = Onyx::Event_Count;
    Onyx::EventType LastEnter = Onyx::Event_Count;
    Onyx::EventType LastMinimize = Onyx::Event_Count;

    const char *LastKey = "none";
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

struct App
{
    Onyx::Window *Window = nullptr;
    Onyx::Overlay *Ui = nullptr;

    // cameras live here, render views only keep a pointer to them
    Onyx::Camera<D2> PixelCam{};
    Onyx::Camera<D2> GalleryCam{};
    Onyx::Camera<D2> TexturesCam{};
    Onyx::Camera<D2> RtCam{};
    Onyx::Camera<D3> Cam3{};

    Onyx::RenderView<D2> *PixelView = nullptr;
    Onyx::RenderView<D2> *GalleryView = nullptr;
    Onyx::RenderView<D3> *View3 = nullptr;
    Onyx::Camera<D2> LabelCam{};
    Onyx::RenderView<D2> *Label3 = nullptr; // transparent 2D view on top of the 3D gallery (title, brackets, cursor)
    Onyx::RenderView<D2> *TexturesView = nullptr;
    Onyx::RenderView<D2> *RtScaledView = nullptr;
    Onyx::RenderView<D2> *RtFixedView = nullptr;

    Onyx::RenderContext<D2> *PixelCtx = nullptr;
    Onyx::RenderContext<D2> *GalleryCtx = nullptr;
    Onyx::RenderContext<D3> *Ctx3 = nullptr;
    Onyx::RenderContext<D2> *LabelCtx = nullptr;
    Onyx::RenderContext<D2> *TexturesCtx = nullptr;
    Onyx::RenderContext<D2> *RtCtx = nullptr;
    Onyx::RenderContext<D2> *ScratchCtx = nullptr; // never renders. used to test context state logic

    Onyx::RenderTexture *RtScaled = nullptr;
    Onyx::RenderTexture *RtFixed = nullptr;

    Onyx::Resource Lit2 = Onyx::NullHandle;
    Onyx::Resource Occluder2 = Onyx::NullHandle;
    Onyx::Resource Mat3 = Onyx::NullHandle;
    Onyx::Resource ProbeMaterial = Onyx::NullHandle; // registered with known values, read back by a check

    RenderSettings Render{};
    f32v2 LightPos2{0.f}; // where the 2D gallery asked its point light to be, in world space
    InputTracking Input{};

    // time
    f32 Time = 0.f;
    f32 DeltaTime = 0.f;
    TKit::FixedArray<f32, 96> FrameTimes{};
    u32 FrameTimeHead = 0;
    TKit::Clock MonotonicClock{};
    f32 LastClockReading = 0.f;
    bool ClockWentBackwards = false;

    // dpi and swapchain tracking
    TKit::StaticArray<ScaleEvent, 10> ScaleEvents{};
    u32 SwapchainRecreations = 0;
    f32 ExtentMismatchSince = -1.f;
    f32 LongestExtentMismatch = 0.f;

    // window properties set by us, read back by checks
    char Title[128]{};
    u32 PresentMode = Onyx::PresentMode_VSync;
    f32 Opacity = 1.f;

    // overlay playground state
    u32 ButtonClicks = 0;
    bool PlaygroundBool = true;
    f32 PlaygroundSlider = 0.5f;
    i32 PlaygroundDrag = 42;
    u32 PlaygroundChoice = 0;
    Onyx::Color PlaygroundColor = Theme::Accent;
    char PlaygroundText[64] = "Type here";

    // ui state
    bool ShowRules = true;
    bool ShowOverlayDemo = false;
    bool ShowStyleEditor = false;

    TKit::FixedArray<Category, Category_Count> Categories{};
    TKit::StaticArray<FailureRecord, 48> FailureLog{};

    Category &Get(const CategoryId id)
    {
        return Categories[id];
    }
};

// =====================================================================================================================
//  5. SETUP
// =====================================================================================================================

void setupCategories(App &app)
{
    const auto define = [&](const CategoryId id, const char *title, const char *summary, const bool open) {
        Category &cat = app.Get(id);
        cat.Title = title;
        cat.Summary = summary;
        cat.Open = open;
    };
    define(Category_Dpi, "DPI and content scale", "logical vs physical pixels, scale changes, swapchain extent", true);
    define(Category_Views, "Render views and cameras", "viewports, coordinate conversions, camera modes, layers",
           false);
    define(Category_Rendering, "Rendering", "primitives, render modes, blending, lights, context state", false);
    define(Category_Textures, "Render textures", "offscreen targets, their content scale and sampling", false);
    define(Category_Input, "Input and events", "event stream consistency, keyboard, mouse, text input", false);
    define(Category_Resources, "Resources", "default resources, materials, fonts, pools", false);
    define(Category_Toolkit, "Toolkit and math", "tensors, quaternions, transforms, colors, containers, time", false);
    define(Category_Platform, "Platform and device", "monitors, window properties, swapchain, GPU", false);
    define(Category_Overlay, "Overlay", "widgets, windows, docking, the overlay's own view", false);

    Category &dpi = app.Get(Category_Dpi);
    dpi.AddVisual(
        "Pixel grid is pixel perfect at integer scales",
        "Top left view. At 1x and 2x the 8 px grid lines and the 1 px checkerboard must be perfectly crisp and "
        "evenly spaced. At fractional scales (1.25x, 1.5x) one logical pixel covers a fractional number of "
        "physical pixels, so some lines will look slightly thicker than others. That is expected. A blurry, "
        "smeared look (like an upscaled image) is not");
    dpi.AddVisual("Cursor markers sit exactly under the mouse",
                  "Every 2D view draws a green ring at the mouse position. It must sit right under the cursor in all "
                  "of them, especially near the right and bottom edges, and in the bottom right view, whose viewport "
                  "does not start at the window origin. Drift that grows towards the bottom right means logical and "
                  "physical coordinates are being mixed");
    dpi.AddVisual("Corner brackets are visible in every view",
                  "Each 2D view draws orange brackets in its four corners. All of them must be fully visible after "
                  "resizing or changing the scale. Missing brackets on the right or bottom means the viewport or "
                  "scissor is computed in the wrong pixel space");
    dpi.AddVisual("Overlay text is sharp at every scale",
                  "The text in these windows must stay sharp after a scale change, not just bigger or smaller");
    dpi.AddVisual("Views re-tile without gaps after a scale change",
                  "The four views have different background tints. After changing the scale they must still cover the "
                  "window exactly, with no black strip or overlap between them");

    Category &views = app.Get(Category_Views);
    views.AddVisual("Cameras only move in the hovered view",
                    "Hover the 2D gallery and press WASD/QE, then do the same over the 3D gallery. Only the hovered "
                    "view's camera must move. The pixel view and the textures view never move");
    views.AddVisual("2D zoom is anchored at the cursor",
                    "Scroll over the 2D gallery. The world point under the cursor must stay under the cursor while "
                    "zooming");
    views.AddVisual("Overlay stays on top",
                    "These windows must always be drawn above the four scene views, wherever you drag them, including "
                    "over the 3D gallery. Layers order views of both dimensions together: a 3D view drawn over the "
                    "overlay means the compositor orders 2D and 3D views separately");
    views.AddVisual("3D gallery title, brackets and cursor are visible",
                    "They are drawn by a transparent 2D view stacked on top of the 3D gallery. If they are missing, "
                    "2D views are being composited below 3D views regardless of their layer");

    Category &rendering = app.Get(Category_Rendering);
    rendering.AddVisual("Every 2D primitive is drawn",
                        "2D gallery, top row: triangle, quad, circle (hollow, partial arc), stadium, rounded rect");
    rendering.AddVisual("Every 3D primitive is drawn",
                        "3D gallery: box, sphere, cylinder, capsule, rounded box and torus over a floor");
    rendering.AddVisual("Outlines are uniform",
                        "With outlines on, every shape gets an even outline that follows its silhouette, with no gaps "
                        "or spikes at corners. Changing the outline width must change all of them");
    rendering.AddVisual("Transparency blends in order",
                        "2D gallery, middle row: three overlapping translucent squares. You must see every overlap "
                        "region tinted by both colors, and the alpha slider must fade all three");
    rendering.AddVisual("Circle parameters are respected",
                        "The circle follows the hollowness, arc and fade sliders exactly");
    rendering.AddVisual("2D light follows the cursor and casts shadows",
                        "With 2D lights on, a warm light follows the cursor in the 2D gallery, centered on the green "
                        "cursor ring, lighting the dark band at the bottom. The spinning occluder must cast a shadow "
                        "away from the light");
    rendering.AddVisual("No light leaks through 2D occluders",
                        "Move the light all around the spinning occluder, especially to its right (the occluder is "
                        "then at angle 180 degrees from the light, where the circular shadow map wraps around). No "
                        "slit of light may cross the occluder or its shadow at any angle");
    rendering.AddVisual("3D point light is visible and its shadows are complete",
                        "A small yellow sphere marks the orbiting point light. Its shadows must stay solid wherever "
                        "the light is: no hole opening from the middle of a shadow as the light moves away. Shadows "
                        "may only end beyond the light's ShadowRadius");
    rendering.AddVisual(
        "3D shadows are correct",
        "With shadows on, every 3D shape casts a shadow onto the floor below it, displaced consistently "
        "with the light direction and following the shape as it rotates. No shadow acne (striped self "
        "shadowing on lit faces), no shadows cut off at cascade boundaries while moving the camera");
    rendering.AddVisual("Text renders crisp",
                        "Text inside the scene views (titles) must be crisp and correctly kerned");

    Category &textures = app.Get(Category_Textures);
    textures.AddVisual("Scaled render texture stays sharp",
                       "Bottom right view and the image in this panel: the left render texture follows the window "
                       "content scale and must look as crisp as the pixel view. The right one is fixed at scale 1 and "
                       "must look softer above 1x. At 1x both must look identical");

    Category &input = app.Get(Category_Input);
    input.AddVisual("Typing reaches the overlay",
                    "Click the text field in the Overlay panel and type. Characters must appear in order, and the "
                    "typed text log in this panel must match");
    input.AddVisual("Overlay captures the mouse",
                    "Dragging an overlay window over a scene view must not move that view's camera, and scrolling "
                    "inside an overlay window must not zoom the scene");

    Category &overlay = app.Get(Category_Overlay);
    overlay.AddVisual("Widgets respond", "In the playground below: the button counts clicks, the checkbox toggles, the "
                                         "slider and drag change their values, the drop down selects");
    overlay.AddVisual("Windows move, resize and collapse",
                      "Drag any window by its header, resize it from its borders and collapse it from its arrow");
    overlay.AddVisual("Docking works",
                      "Drag a panel over another one: dock previews must appear, and dropping must dock it. Dragging "
                      "the tab away must undock it again");
    overlay.AddVisual("Tooltips and popups",
                      "Hover the playground button for a tooltip, right click it for a popup menu");
}

void setup(App &app)
{
    // -- resources --------------------------------------------------------------------------------------------------
    // materials are registered up front and synced once. Resources_Sync uploads everything registered so far
    app.Lit2 = Onyx::Material_Register<D2>();
    app.Occluder2 = Onyx::Material_Register<D2>({.Occluder = true});
    app.Mat3 = Onyx::Material_Register<D3>({.RoughnessFactor = 0.35f});
    app.ProbeMaterial =
        Onyx::Material_Register<D2>({.ColorFactor = Theme::Accent, .TexOffset = f32v2{0.25f}, .TexScale = f32v2{2.f}});
    Onyx::Resources_Sync(Onyx::SyncFlag_Materials);

    // -- window -----------------------------------------------------------------------------------------------------
    app.Window = Onyx::OpenWindow({.Window = {.Title = "Onyx control center",
                                              .ScreenSize = InitialWindowSize,
                                              .PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Window *win = app.Window;

    const Graph::PlatformBackendInfo pinfo = Graph::GetPlatformBackendInfo();
    const Line title = Format("Onyx control center | {}", platformName(pinfo.Platform));
    for (u32 i = 0; i <= title.Size && i < sizeof(app.Title) - 1; ++i)
        app.Title[i] = title.Data[i];
    win->SetTitle(app.Title);

    // -- scene views ------------------------------------------------------------------------------------------------
    // all four use proportional viewports, so they follow the window when it is resized or its scale changes
    const auto proportional = [](const u32 quadrant) {
        return Onyx::Viewport{.Position = {Quadrants[quadrant].X, Quadrants[quadrant].Y},
                              .Size = {QuadrantSize, QuadrantSize}};
    };

    app.PixelCam.Mode = Onyx::CameraMode_Viewport;
    app.PixelView = win->CreateRenderView<D2>(
        &app.PixelCam, {.Flags = Onyx::RenderViewFlag_PostProcess | Onyx::RenderViewFlag_Outlines});
    app.PixelView->SetProportionalViewport(proportional(0));
    app.PixelView->ClearColor = Theme::ClearPixel;

    app.GalleryCam.OrthoParameters.Size = 10.f;
    app.GalleryView = win->CreateRenderView<D2>(
        &app.GalleryCam, {.Flags = Onyx::RenderViewFlag_Shadows | Onyx::RenderViewFlag_PostProcess |
                                   Onyx::RenderViewFlag_Outlines | Onyx::RenderViewFlag_Transparency});
    app.GalleryView->SetProportionalViewport(proportional(1));
    app.GalleryView->ClearColor = Theme::ClearGallery;

    app.Cam3.View.Translation = f32v3{0.f, 1.5f, 9.5f};
    app.View3 = win->CreateRenderView<D3>(&app.Cam3,
                                          {.Flags = Onyx::RenderViewFlag_Shadows | Onyx::RenderViewFlag_PostProcess |
                                                    Onyx::RenderViewFlag_Outlines | Onyx::RenderViewFlag_Transparency});
    app.View3->SetProportionalViewport(proportional(2));
    app.View3->ClearColor = Theme::ClearScene3;

    // 2D views can sit on top of 3D ones: this transparent view shares the 3D gallery's viewport and draws its title,
    // corner brackets and cursor ring. it only shows if 2D and 3D views are composited by layer. the clear color is
    // transparent black, like the overlay's
    app.LabelCam.Mode = Onyx::CameraMode_Viewport;
    app.Label3 = win->CreateRenderView<D2>(&app.LabelCam);
    app.Label3->SetProportionalViewport(proportional(2));
    app.Label3->ClearColor = Onyx::Color{0.f, 0.f, 0.f, 0.f};

    app.TexturesCam.Mode = Onyx::CameraMode_Viewport;
    app.TexturesView = win->CreateRenderView<D2>(&app.TexturesCam);
    app.TexturesView->SetProportionalViewport(proportional(3));
    app.TexturesView->ClearColor = Theme::ClearTextures;

    // -- render textures --------------------------------------------------------------------------------------------
    // one follows the window content scale (updated on Event_ContentScaleChanged), the other stays at 1 as a reference
    app.RtScaled = Onyx::CreateRenderTexture(RenderTextureSize, {.ContentScale = win->GetContentScale()[0]});
    app.RtFixed = Onyx::CreateRenderTexture(RenderTextureSize, {.ContentScale = 1.f});
    app.RtCam.Mode = Onyx::CameraMode_Viewport;
    app.RtScaledView = app.RtScaled->CreateRenderView<D2>(&app.RtCam);
    app.RtFixedView = app.RtFixed->CreateRenderView<D2>(&app.RtCam);
    app.RtScaledView->ClearColor = Theme::ClearRenderTexture;
    app.RtFixedView->ClearColor = Theme::ClearRenderTexture;

    // -- render contexts --------------------------------------------------------------------------------------------
    // a context records draw commands. it can target any number of views, of any render target.
    // instance buffers are preallocated for every (blend pass, render mode, resource) combination: 2 x 5 buffers per
    // glyph of every font, each holding MaxInstances instances. scene text here is short, so the glyph limit is kept
    // small to avoid paying for ~100 glyphs x 10 buffers x 1000 instances in every context
    Onyx::RenderContextSpecs sceneSpecs{};
    sceneSpecs.MaxInstances[Onyx::Geometry_Glyph] = 64;
    // the scratch context never draws, so it needs no instance memory at all (any accidental draw asserts loudly)
    Onyx::RenderContextSpecs scratchSpecs{};
    for (u32 &count : scratchSpecs.MaxInstances)
        count = 0;

    app.PixelCtx = Onyx::CreateRenderContext<D2>(sceneSpecs);
    app.PixelCtx->AddTarget(app.PixelView);
    app.GalleryCtx = Onyx::CreateRenderContext<D2>(sceneSpecs);
    app.GalleryCtx->AddTarget(app.GalleryView);
    app.Ctx3 = Onyx::CreateRenderContext<D3>(sceneSpecs);
    app.Ctx3->AddTarget(app.View3);
    app.LabelCtx = Onyx::CreateRenderContext<D2>(sceneSpecs);
    app.LabelCtx->AddTarget(app.Label3);
    app.TexturesCtx = Onyx::CreateRenderContext<D2>(sceneSpecs);
    app.TexturesCtx->AddTarget(app.TexturesView);
    app.RtCtx = Onyx::CreateRenderContext<D2>(sceneSpecs);
    app.RtCtx->AddTarget(app.RtScaledView);
    app.RtCtx->AddTarget(app.RtFixedView);
    app.ScratchCtx = Onyx::CreateRenderContext<D2>(scratchSpecs);

    // -- overlay ----------------------------------------------------------------------------------------------------
    // created last and explicitly brought to the top, so it is drawn above every scene view
    // the overlay limit applies to each glyph separately. the panels here are text heavy, so raise it for glyphs.
    // memory: ~100 glyphs x 10 buffers x 84 bytes x 3000 = ~250 MB of preallocated instance data (see the report)
    Onyx::OverlaySpecs ospecs{.Flags = Onyx::OverlayFlag_Docking};
    ospecs.Context.MaxInstances[Onyx::Geometry_Glyph] = 3000;
    app.Ui = win->CreateOverlay(ospecs);
    win->BringToTop(app.Ui->GetMainNativeWindow()->View);

    setupCategories(app);
}

void resetCameras(App &app)
{
    app.GalleryCam.View = {};
    app.GalleryCam.OrthoParameters.Size = 10.f;
    app.Cam3.View = {};
    app.Cam3.View.Translation = f32v3{0.f, 1.5f, 9.5f};
    app.Cam3.PerspParameters.FieldOfView = Math::Radians(75.f);
}

// =====================================================================================================================
//  6. EVENT PROCESSING AND CAMERA CONTROL
// =====================================================================================================================

// "press" and "release" style events must alternate. two presses in a row (or two releases) is a violation
void checkAlternation(Onyx::EventType &last, const Onyx::EventType current, u32 &violations)
{
    if (last == current)
        ++violations;
    last = current;
}

template <typename Array, typename T> bool removeHeld(Array &held, const T value)
{
    for (T *it = held.begin(); it != held.end(); ++it)
        if (*it == value)
        {
            held.RemoveUnordered(it);
            return true;
        }
    return false;
}

void processEvents(App &app)
{
    InputTracking &in = app.Input;
    Onyx::Window *win = app.Window;
    const f32v2 mouse = win->GetMousePosition();

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
        case Onyx::Event_KeyRepeat:
            if (!in.HeldKeys.Contains(event.Key))
                ++in.RepeatWithoutPress;
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
            if (!removeHeld(in.HeldButtons, event.Mouse.Button))
                ++in.ReleaseWithoutPress;
            break;
        case Onyx::Event_MouseEntered:
        case Onyx::Event_MouseLeft:
            checkAlternation(in.LastEnter, event.Type, in.EnterAlternation);
            break;
        case Onyx::Event_WindowFocused:
        case Onyx::Event_WindowUnfocused:
            checkAlternation(in.LastFocus, event.Type, in.FocusAlternation);
            break;
        case Onyx::Event_WindowMinimized:
        case Onyx::Event_WindowRestored:
            checkAlternation(in.LastMinimize, event.Type, in.MinimizeAlternation);
            break;
        case Onyx::Event_Scrolled:
            in.LastScroll = event.ScrollOffset;
            if (app.Ui->WantCaptureScroll())
                break;
            if (app.GalleryView->IsWithinViewport(mouse))
                app.GalleryView->ZoomScroll(mouse, 0.1f * event.ScrollOffset[1]);
            else if (app.View3->IsWithinViewport(mouse))
                app.View3->ZoomScroll(f32v3{mouse, 0.f}, 0.05f * event.ScrollOffset[1]);
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

// only the hovered view's camera moves, and never while the overlay wants the keyboard. note that
// Window::GetMouseRenderView<D2>() is not usable here: the overlay's own view is a 2D view covering the whole window,
// and it is always on top
void controlCameras(App &app)
{
    if (app.Ui->WantCaptureKeyboard() || app.Ui->WantCaptureMouse())
        return;
    const f32v2 mouse = app.Window->GetMousePosition();
    const TKit::Timespan dt = Onyx::GetDeltaTime(app.Window);
    if (app.GalleryView->IsWithinViewport(mouse))
        app.Window->ControlCamera(dt, &app.GalleryCam);
    else if (app.View3->IsWithinViewport(mouse))
        app.Window->ControlCamera(dt, &app.Cam3);
}

// =====================================================================================================================
//  7. AUTOMATIC CHECKS
// =====================================================================================================================

void checkDpi(App &app)
{
    Category &cat = app.Get(Category_Dpi);
    Onyx::Window *win = app.Window;
    const Graph::Window handle = win->GetHandle();

    const f32v2 scale = win->GetContentScale();
    const u32v2 size = win->GetSize();
    const u32v2 pixels = Graph::Window_GetPixelSize(handle);
    const u32v2 expected = Onyx::ApplyScale(size, scale);
    const u32v2 extent = Graph::Swapchain_GetExtent(win->GetSwapchain());

    cat.Expect("Content scale is positive", scale[0] > 0.f && scale[1] > 0.f,
               "The content scale is the ratio between physical and logical pixels. It can never be zero or negative",
               "scale = {:.3f} x {:.3f}", scale[0], scale[1]);
    cat.Expect("Content scale is uniform", near(scale[0], scale[1], Epsilon),
               "Desktop platforms report the same scale on both axes. Onyx relies on the x component in a few places "
               "(render texture scale, outline width), so a non uniform scale would be handled incorrectly",
               "x = {:.3f}, y = {:.3f}", scale[0], scale[1]);
    cat.Expect("Pixel size = logical size x scale", nearPixels(pixels, expected, 1),
               "Window::GetSize() returns logical pixels. Multiplying by the content scale must give the framebuffer "
               "size in physical pixels (rounding may differ by one)",
               "logical {} x {}  x {:.3f}  = {} x {}, framebuffer {} x {}", size[0], size[1], scale[0], expected[0],
               expected[1], pixels[0], pixels[1]);
    cat.Expect("Logical size survives a scale round trip",
               nearPixels(Onyx::UnapplyScale(Onyx::ApplyScale(size, scale), scale), size, 1),
               "ApplyScale and UnapplyScale convert between logical and physical sizes. Going there and back must "
               "return the original size (within rounding)",
               "{} x {} -> {} x {} -> back", size[0], size[1], expected[0], expected[1]);

    // the swapchain is recreated lazily, so it may lag for a few frames. it must not stay out of sync
    const bool mismatch = extent != pixels;
    if (mismatch && app.ExtentMismatchSince < 0.f)
        app.ExtentMismatchSince = app.Time;
    else if (!mismatch)
        app.ExtentMismatchSince = -1.f;
    const f32 mismatchFor = mismatch ? app.Time - app.ExtentMismatchSince : 0.f;
    app.LongestExtentMismatch = Math::Max(app.LongestExtentMismatch, mismatchFor);
    cat.Expect("Swapchain extent follows the framebuffer", mismatchFor <= SwapchainGracePeriod,
               "The swapchain images must have the framebuffer size in physical pixels. After a resize or a scale "
               "change, the swapchain is recreated and must catch up within a short grace period",
               "swapchain {} x {}, framebuffer {} x {}, out of sync for {:.2f}s (longest so far {:.2f}s)", extent[0],
               extent[1], pixels[0], pixels[1], mismatchFor, app.LongestExtentMismatch);

    bool viewsOk = true;
    for (const Onyx::RenderView<D2> *rv : win->GetRenderViews<D2>())
        viewsOk &= near(rv->GetContentScale(), scale, Epsilon);
    for (const Onyx::RenderView<D3> *rv : win->GetRenderViews<D3>())
        viewsOk &= near(rv->GetContentScale(), scale, Epsilon);
    cat.Expect("Every window view uses the window scale", viewsOk,
               "Render views size their framebuffers as logical size x content scale. A window view still holding an "
               "old scale renders at the wrong resolution (blurry or clipped)",
               "{} 2D views, {} 3D views checked against {:.3f}", win->GetRenderViews<D2>().GetSize(),
               win->GetRenderViews<D3>().GetSize(), scale[0]);

    if (!app.ScaleEvents.IsEmpty())
    {
        const f32v2 last = app.ScaleEvents.GetBack().Scale;
        cat.Expect("Last scale event matches the current scale", near(last, scale, Epsilon),
                   "Event_ContentScaleChanged carries the new scale. The most recent one must equal what the window "
                   "reports now, otherwise an event was lost or carries a stale value",
                   "last event {:.3f} x {:.3f}, window {:.3f} x {:.3f}", last[0], last[1], scale[0], scale[1]);
    }

    if (win->IsHovered())
    {
        const f32v2 mouse = win->GetMousePosition();
        const bool inside =
            mouse[0] >= -1.f && mouse[1] >= -1.f && mouse[0] <= f32(size[0]) + 1.f && mouse[1] <= f32(size[1]) + 1.f;
        cat.Expect("Hovered mouse lies inside the logical window", inside,
                   "Window::GetMousePosition() returns logical pixels. While the cursor hovers the window it must lie "
                   "within [0, logical size]. A position up to scale times too large means it is in physical pixels",
                   "mouse ({:.1f}, {:.1f}) in {} x {}", mouse[0], mouse[1], size[0], size[1]);
    }
}

void checkViews(App &app)
{
    Category &cat = app.Get(Category_Views);
    Onyx::Window *win = app.Window;
    const f32v2 wsize = f32v2{win->GetSize()};
    const f32v2 mouse = win->GetMousePosition();

    // the four quadrant views, as 2D or 3D, share most of their checks
    const auto viewportOf = [&](const u32 quadrant) -> const Onyx::Viewport & {
        switch (quadrant)
        {
        case 0:
            return app.PixelView->GetViewport();
        case 1:
            return app.GalleryView->GetViewport();
        case 2:
            return app.View3->GetViewport();
        default:
            return app.TexturesView->GetViewport();
        }
    };
    const auto imageSizeOf = [&](const u32 quadrant) -> u32v2 {
        switch (quadrant)
        {
        case 0:
            return app.PixelView->GetImageSize();
        case 1:
            return app.GalleryView->GetImageSize();
        case 2:
            return app.View3->GetImageSize();
        default:
            return app.TexturesView->GetImageSize();
        }
    };

    f32 area = 0.f;
    for (u32 q = 0; q < 4; ++q)
    {
        const Onyx::Viewport &vp = viewportOf(q);
        area += vp.Size[0] * vp.Size[1];
        const f32v2 expectedPos = f32v2{Quadrants[q].X, Quadrants[q].Y} * wsize;
        const f32v2 expectedSize = QuadrantSize * wsize;
        cat.Expect(ViewportCheckNames[q], near(vp.Position, expectedPos, 0.5f) && near(vp.Size, expectedSize, 0.5f),
                   "A proportional viewport is stored as window fractions and must be recomputed from the logical "
                   "window size on every resize and scale change. Position and size are in logical pixels",
                   "viewport ({:.1f}, {:.1f}) {:.1f} x {:.1f}, expected ({:.1f}, {:.1f}) {:.1f} x {:.1f}",
                   vp.Position[0], vp.Position[1], vp.Size[0], vp.Size[1], expectedPos[0], expectedPos[1],
                   expectedSize[0], expectedSize[1]);

        const u32v2 isize = imageSizeOf(q);
        cat.Expect(ImageCheckNames[q], nearPixels(isize, u32v2{Math::Round(vp.Size)}, 1),
                   "With proportional image sizing (the default, ratio 1) a view's image has the logical size of its "
                   "viewport. The physical extent is that size times the content scale",
                   "image {} x {}, viewport {:.0f} x {:.0f}", isize[0], isize[1], vp.Size[0], vp.Size[1]);
    }
    cat.Expect("Views tile the window", near(area, wsize[0] * wsize[1], 2.f * (wsize[0] + wsize[1])),
               "The four quadrant viewports must cover the window exactly: their areas add up to the window area "
               "(within a pixel of rounding along the seams)",
               "viewports {:.0f} px2, window {:.0f} px2", area, wsize[0] * wsize[1]);

    // coordinate conversions
    const auto roundTrip2 = [&](const char *name, const Onyx::RenderView<D2> *rv) {
        const Onyx::Viewport &vp = rv->GetViewport();
        const f32v2 probes[3] = {vp.Position + 0.25f * vp.Size, vp.Position + 0.75f * vp.Size,
                                 rv->IsWithinViewport(mouse) ? mouse : vp.Position + 0.5f * vp.Size};
        f32 maxError = 0.f;
        for (const f32v2 &p : probes)
            maxError = Math::Max(maxError, Math::Norm(rv->WorldToScreen(rv->ScreenToWorld(p)) - p));
        cat.Expect(name, maxError < 0.01f,
                   "ScreenToWorld and WorldToScreen are inverses. Converting a screen point to world space and back "
                   "must land on the same logical pixel, for any camera position, rotation or zoom",
                   "max error {:.5f} px over 3 probes", maxError);
    };
    roundTrip2("Pixel view: screen -> world -> screen", app.PixelView);
    roundTrip2("2D gallery: screen -> world -> screen", app.GalleryView);
    roundTrip2("Textures: screen -> world -> screen", app.TexturesView);

    {
        const Onyx::Viewport &vp = app.View3->GetViewport();
        const f32v2 p = app.View3->IsWithinViewport(mouse) ? mouse : vp.Position + 0.5f * vp.Size;
        const f32v3 world = app.View3->ScreenToWorld(f32v3{p, 0.5f});
        const f32 error = Math::Norm(app.View3->WorldToScreen(world) - p);
        cat.Expect("3D gallery: screen -> world -> screen", error < 0.05f,
                   "In 3D, a screen point plus a depth (in normalized device coordinates) defines a world point. "
                   "Projecting it back must give the same screen point",
                   "error {:.5f} px at depth 0.5", error);
    }

    {
        const Onyx::Viewport &vp = app.PixelView->GetViewport();
        const f32v2 p = vp.Position + 0.5f * vp.Size;
        const f32v2 dx = app.PixelView->ScreenToWorld(p + f32v2{1.f, 0.f}) - app.PixelView->ScreenToWorld(p);
        const f32v2 dy = app.PixelView->ScreenToWorld(p + f32v2{0.f, 1.f}) - app.PixelView->ScreenToWorld(p);
        cat.Expect("Viewport camera: 1 world unit = 1 logical pixel",
                   near(Math::Norm(dx), 1.f, 1e-3f) && near(Math::Norm(dy), 1.f, 1e-3f),
                   "CameraMode_Viewport maps one world unit to one logical pixel, independently of the content scale. "
                   "Moving one logical pixel on screen must move exactly one unit in world space",
                   "1 px right -> ({:.4f}, {:.4f}), 1 px down -> ({:.4f}, {:.4f})", dx[0], dx[1], dy[0], dy[1]);
    }
    {
        const Onyx::Viewport &vp = app.GalleryView->GetViewport();
        const f32v2 top = app.GalleryView->ScreenToWorld(vp.Position + f32v2{0.5f * vp.Size[0], 0.f});
        const f32v2 bottom = app.GalleryView->ScreenToWorld(vp.Position + f32v2{0.5f * vp.Size[0], vp.Size[1]});
        const f32 span = Math::Norm(top - bottom);
        cat.Expect("Orthographic camera: viewport height = ortho size",
                   near(span, app.GalleryCam.OrthoParameters.Size, 1e-2f),
                   "An orthographic camera shows OrthoParameters.Size world units from the top to the bottom of its "
                   "viewport, whatever the window size or scale. Zooming changes that size",
                   "top to bottom = {:.4f} units, ortho size = {:.4f}", span, app.GalleryCam.OrthoParameters.Size);

        const f32v2 center = app.GalleryView->ScreenToWorld(vp.Position + 0.5f * vp.Size);
        const f32v2 camPos = app.GalleryCam.View.Translation;
        cat.Expect("Viewport center = camera position", near(center, camPos, 1e-3f),
                   "A 2D camera looks at its own position, so the center of its viewport maps to the camera "
                   "translation, even after moving or rotating it (WASD/QE)",
                   "center ({:.3f}, {:.3f}), camera ({:.3f}, {:.3f})", center[0], center[1], camPos[0], camPos[1]);
    }

    const auto sorted = win->GetSortedViews<D2>();
    bool ordered = true;
    for (u32 i = 1; i < sorted.GetSize(); ++i)
        ordered &= sorted[i - 1]->Layer <= sorted[i]->Layer;
    cat.Expect("Sorted views are ordered by layer", ordered,
               "Views are drawn in increasing Layer order. GetSortedViews must return them that way, which is what "
               "BringToTop and BringToBottom rely on",
               "{} 2D views", sorted.GetSize());
}

void checkRendering(App &app)
{
    Category &cat = app.Get(Category_Rendering);
    Onyx::RenderContext<D2> *ctx = app.ScratchCtx;

    // the scratch context has no targets. it is used only to exercise state logic, and flushed every frame
    ctx->Flush();
    const Onyx::ContextState<D2> initial = ctx->GetState();
    cat.Expect("Flush resets the context state",
               initial.FillColor == Onyx::Color_White && Math::Approximately(initial.Transform, f32m3::Identity()) &&
                   initial.RenderFlags == Onyx::RenderModeFlag_Flat,
               "Flush() clears recorded draws and returns the state to its defaults: white fill, identity transform, "
               "flat render mode. Every frame starts from there",
               "fill alpha {:.2f}, render flags {}", initial.FillColor.rgba[3], initial.RenderFlags);

    ctx->Push();
    ctx->FillColor(Onyx::Color_Red);
    ctx->Translate(f32v2{3.f, -2.f});
    ctx->Pop();
    cat.Expect("Pop restores what Push saved",
               ctx->GetState().FillColor == initial.FillColor &&
                   Math::Approximately(ctx->GetState().Transform, initial.Transform),
               "Push() saves the whole state (transform, colors, material, flags...) and Pop() restores it, so changes "
               "made in between never leak to later draws",
               "fill and transform compared after Push, FillColor, Translate, Pop");

    ctx->Translate(f32v2{1.f, 0.f});
    ctx->Scale(2.f);
    const f32v2 extrinsic = Onyx::Transform<D2>::ExtractTranslation(ctx->GetState().Transform);
    ctx->ResetTransform();
    ctx->Translate(f32v2{1.f, 0.f});
    ctx->Scale(2.f, Onyx::Transform_Intrinsic);
    const f32v2 intrinsic = Onyx::Transform<D2>::ExtractTranslation(ctx->GetState().Transform);
    ctx->ResetTransform();
    cat.Expect("Extrinsic vs intrinsic transforms",
               near(extrinsic, f32v2{2.f, 0.f}, Epsilon) && near(intrinsic, f32v2{1.f, 0.f}, Epsilon),
               "Transforms are extrinsic by default: each one is applied in world space on top of the previous ones, "
               "so Translate(1, 0) then Scale(2) scales the translation too and the object ends at (2, 0). With "
               "Transform_Intrinsic the scale is applied in local space and the object stays at (1, 0). Rule of thumb "
               "for extrinsic: scale, then rotate, then translate",
               "extrinsic -> ({:.3f}, {:.3f}), intrinsic -> ({:.3f}, {:.3f})", extrinsic[0], extrinsic[1], intrinsic[0],
               intrinsic[1]);

    ctx->Scale(f32v2{2.f, 3.f});
    ctx->Rotate(0.7f);
    ctx->Translate(f32v2{4.f, 5.f});
    f32v2 t, s;
    f32 r;
    Onyx::Transform<D2>::Extract(ctx->GetState().Transform, &t, &s, &r);
    cat.Expect("Scale, rotate, translate can be extracted back",
               near(t, f32v2{4.f, 5.f}, 1e-3f) && near(s, f32v2{2.f, 3.f}, 1e-3f) && near(r, 0.7f, 1e-3f),
               "Composing scale (2, 3), rotation 0.7 rad and translation (4, 5) and then decomposing the resulting "
               "matrix must give the same three components back",
               "t = ({:.3f}, {:.3f}), s = ({:.3f}, {:.3f}), r = {:.3f}", t[0], t[1], s[0], s[1], r);

    const f32v2 local{0.3f, -1.2f};
    const f32v2 back = ctx->WorldToLocal(ctx->LocalToWorld(local));
    cat.Expect("LocalToWorld and WorldToLocal are inverses", near(back, local, 1e-3f),
               "Converting a point from the current local space to world space and back must return the same point",
               "({:.3f}, {:.3f}) -> world -> ({:.3f}, {:.3f})", local[0], local[1], back[0], back[1]);
    ctx->ResetTransform();

    ctx->Alpha(0.25f);
    const bool translucent = ctx->GetState().Blend == Onyx::BlendPass_Transparent;
    ctx->Alpha(1.f);
    const bool opaque = ctx->GetState().Blend == Onyx::BlendPass_Opaque;
    ctx->FillColor(Onyx::Color{Onyx::Color_Sky, 0.5f});
    const bool fromColor = ctx->GetState().Blend == Onyx::BlendPass_Transparent;
    cat.Expect("Translucent fills go to the transparent pass", translucent && opaque && fromColor,
               "Alpha() or a FillColor with alpha below 1 routes the following draws to the transparent blend pass, "
               "which sorts and blends them. Alpha 1 routes them back to the opaque pass. Views need "
               "RenderViewFlag_Transparency for the transparent pass to be composited",
               "Alpha(0.25) -> {}, Alpha(1) -> {}, FillColor(a = 0.5) -> {}", translucent ? "transparent" : "opaque",
               opaque ? "opaque" : "transparent", fromColor ? "transparent" : "opaque");
    ctx->Flush();

    const u32 points2 = app.GalleryCtx->GetPointLightParams().GetSize();
    cat.Expect("2D context recorded its lights", points2 == (app.Render.Lights2 ? 1u : 0u),
               "Lights are recorded per context, like draws. The 2D gallery adds one point light (following the "
               "cursor) when 2D lights are enabled",
               "{} point lights recorded, {} expected", points2, app.Render.Lights2 ? 1 : 0);

    if (points2 == 1)
    {
        const f32v2 recorded = app.GalleryCtx->GetPointLightParams()[0].Position;
        cat.Expect("2D light lands where it was asked for", near(recorded, app.LightPos2, 1e-3f),
                   "PointLight() transforms the light position by the current context transform, like any draw. The "
                   "2D gallery emits its light with an identity transform, so the recorded position must equal the "
                   "requested world position (the cursor). A mismatch means the light was emitted under some other "
                   "object's transform",
                   "recorded ({:.3f}, {:.3f}), requested ({:.3f}, {:.3f})", recorded[0], recorded[1], app.LightPos2[0],
                   app.LightPos2[1]);
    }

    const u32 points3 = app.Ctx3->GetPointLightParams().GetSize();
    const u32 spots3 = app.Ctx3->GetSpotLightParams().GetSize();
    const u32 dirs3 = app.Ctx3->GetDirectionalLightParams().GetSize();
    const bool lit3 = app.Render.Lights3;
    const u32 expectedPoints = lit3 && app.Render.PointLight3 ? 1 : 0;
    const u32 expectedSpots = lit3 && app.Render.SpotLight3 ? 1 : 0;
    cat.Expect("3D context recorded its lights",
               points3 == expectedPoints && spots3 == expectedSpots && dirs3 == (lit3 ? 1u : 0u),
               "The 3D gallery records one directional light (with cascaded shadows bound to its view), plus an "
               "optional point light and spot light",
               "directional {}, point {} (expected {}), spot {} (expected {})", dirs3, points3, expectedPoints, spots3,
               expectedSpots);

    Onyx::RenderViewFlags wanted = Onyx::RenderViewFlag_Transparency;
    if (app.Render.Outlines)
        wanted |= Onyx::RenderViewFlag_Outlines;
    if (app.Render.Shadows)
        wanted |= Onyx::RenderViewFlag_Shadows;
    if (app.Render.PostProcess)
        wanted |= Onyx::RenderViewFlag_PostProcess;
    const Onyx::RenderViewFlags mask = Onyx::RenderViewFlag_Outlines | Onyx::RenderViewFlag_Shadows |
                                       Onyx::RenderViewFlag_PostProcess | Onyx::RenderViewFlag_Transparency;
    cat.Expect("Gallery view flags match the toggles",
               (app.GalleryView->GetFlags() & mask) == wanted && (app.View3->GetFlags() & mask) == wanted,
               "RenderView::SetFlags enables or disables whole passes. Toggling post process or transparency "
               "recreates the view's framebuffers. The flags read back must be the ones requested",
               "2D gallery flags {:#06x}, 3D gallery flags {:#06x}, wanted {:#06x}", app.GalleryView->GetFlags() & mask,
               app.View3->GetFlags() & mask, wanted);
}

void checkTextures(App &app)
{
    Category &cat = app.Get(Category_Textures);
    const f32v2 wscale = app.Window->GetContentScale();

    const auto checkOne = [&](const char *sizeName, const char *extentName, const Onyx::RenderTexture *rt,
                              const Onyx::RenderView<D2> *rv) {
        cat.Expect(sizeName, rt->GetSize() == RenderTextureSize && rv->GetImageSize() == RenderTextureSize,
                   "A render texture is a render target with its own logical size. Its views use proportional sizing "
                   "too, so a full size view has the texture's logical size",
                   "texture {} x {}, view image {} x {}", rt->GetSize()[0], rt->GetSize()[1], rv->GetImageSize()[0],
                   rv->GetImageSize()[1]);

        const u32v2 expected = Onyx::ApplyScale(RenderTextureSize, rt->GetContentScale());
        cat.Expect(extentName,
                   rv->GetImageExtent() == expected && near(rv->GetContentScale(), rt->GetContentScale(), Epsilon),
                   "The physical size of a render texture is its logical size times its own content scale, and that "
                   "scale must reach its views (they recreate their framebuffers when it changes)",
                   "scale {:.3f}, extent {} x {}, expected {} x {}", rt->GetContentScale()[0], rv->GetImageExtent()[0],
                   rv->GetImageExtent()[1], expected[0], expected[1]);
    };
    checkOne("Scaled texture: logical size", "Scaled texture: physical extent", app.RtScaled, app.RtScaledView);
    checkOne("Fixed texture: logical size", "Fixed texture: physical extent", app.RtFixed, app.RtFixedView);

    cat.Expect(
        "Scaled texture follows the window scale", near(app.RtScaled->GetContentScale(), wscale, Epsilon),
        "This demo forwards every Event_ContentScaleChanged to RenderTexture::SetContentScale. If they disagree, "
        "the event was not delivered or SetContentScale did not apply",
        "texture {:.3f}, window {:.3f}", app.RtScaled->GetContentScale()[0], wscale[0]);
    cat.Expect("Fixed texture keeps scale 1", near(app.RtFixed->GetContentScale(), f32v2{1.f}, Epsilon),
               "The reference texture is never rescaled", "scale {:.3f} x {:.3f}", app.RtFixed->GetContentScale()[0],
               app.RtFixed->GetContentScale()[1]);
}

void checkInput(App &app)
{
    Category &cat = app.Get(Category_Input);
    InputTracking &in = app.Input; // non const: TKit's Contains is not a const member

    cat.Expect("Key repeats only follow a press", in.RepeatWithoutPress == 0,
               "Event_KeyRepeat is only sent while a key is held, so it must always come after an Event_KeyPressed of "
               "the same key, with no Event_KeyReleased in between",
               "{} violations since start, {} keys held now", in.RepeatWithoutPress, in.HeldKeys.GetSize());
    cat.Expect("Mouse releases only follow a press", in.ReleaseWithoutPress == 0,
               "Every Event_MouseReleased must match an earlier Event_MousePressed of the same button. (Pressing "
               "outside the window and releasing inside can legitimately break this on some platforms)",
               "{} violations since start, {} buttons held now", in.ReleaseWithoutPress, in.HeldButtons.GetSize());
    cat.Expect("Focus events alternate", in.FocusAlternation == 0,
               "A window cannot gain focus twice without losing it in between: focused and unfocused events must "
               "alternate",
               "{} violations, {} focused, {} unfocused", in.FocusAlternation,
               in.EventCounts[Onyx::Event_WindowFocused], in.EventCounts[Onyx::Event_WindowUnfocused]);
    cat.Expect("Enter and leave events alternate", in.EnterAlternation == 0,
               "The cursor cannot enter the window twice without leaving it in between",
               "{} violations, {} entered, {} left", in.EnterAlternation, in.EventCounts[Onyx::Event_MouseEntered],
               in.EventCounts[Onyx::Event_MouseLeft]);
    cat.Expect("Minimize and restore events alternate", in.MinimizeAlternation == 0,
               "A window cannot be minimized twice without being restored in between",
               "{} violations, {} minimized, {} restored", in.MinimizeAlternation,
               in.EventCounts[Onyx::Event_WindowMinimized], in.EventCounts[Onyx::Event_WindowRestored]);
    cat.Expect("Polled buttons agree with the event stream",
               !app.Window->IsMousePressed(Onyx::Mouse_ButtonLeft) || in.HeldButtons.Contains(Onyx::Mouse_ButtonLeft) ||
                   !app.Window->IsHovered(),
               "If polling says the left button is down while the cursor is over the window, the event stream must "
               "have delivered the matching press",
               "polled left button {}, held according to events {}",
               app.Window->IsMousePressed(Onyx::Mouse_ButtonLeft) ? "down" : "up",
               in.HeldButtons.Contains(Onyx::Mouse_ButtonLeft) ? "yes" : "no");
}

void checkResources(App &app)
{
    Category &cat = app.Get(Category_Resources);
    const Onyx::DefaultResources &res = Onyx::Resources_GetDefault();

    const bool meshes2 = Onyx::IsResourceValid<D2>(res.Triangle2, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D2>(res.Quad2, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D2>(res.Stadium2, Onyx::Resource_ParametricMesh) &&
                         Onyx::IsResourceValid<D2>(res.RoundedRect2, Onyx::Resource_ParametricMesh);
    cat.Expect("Default 2D meshes are valid", meshes2,
               "Resources_CreateDefault registers the built in 2D meshes: triangle and quad (static), stadium and "
               "rounded rect (parametric, their shape is controlled per instance)",
               "triangle, quad, stadium, rounded rect");

    const bool meshes3 = Onyx::IsResourceValid<D3>(res.Triangle3, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D3>(res.Quad3, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D3>(res.Box, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D3>(res.Sphere, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D3>(res.Cylinder, Onyx::Resource_StaticMesh) &&
                         Onyx::IsResourceValid<D3>(res.Stadium3, Onyx::Resource_ParametricMesh) &&
                         Onyx::IsResourceValid<D3>(res.RoundedRect3, Onyx::Resource_ParametricMesh) &&
                         Onyx::IsResourceValid<D3>(res.Capsule, Onyx::Resource_ParametricMesh) &&
                         Onyx::IsResourceValid<D3>(res.RoundedBox, Onyx::Resource_ParametricMesh) &&
                         Onyx::IsResourceValid<D3>(res.Torus, Onyx::Resource_ParametricMesh);
    cat.Expect("Default 3D meshes are valid", meshes3,
               "The built in 3D meshes: triangle, quad, box, sphere, cylinder (static), stadium, rounded rect, "
               "capsule, rounded box and torus (parametric)",
               "10 meshes checked");

    cat.Expect("Default pools are valid",
               Onyx::IsResourcePoolValid(res.StaticPool2) && Onyx::IsResourcePoolValid(res.ParametricPool2) &&
                   Onyx::IsResourcePoolValid(res.StaticPool3) && Onyx::IsResourcePoolValid(res.ParametricPool3) &&
                   Onyx::IsResourcePoolValid(res.FontPool),
               "Meshes and fonts live in resource pools. The default ones hold the default resources",
               "static 2D: {} meshes, parametric 2D: {}, static 3D: {}, parametric 3D: {}",
               Onyx::ResourcePool_GetResourceCount<D2>(res.StaticPool2),
               Onyx::ResourcePool_GetResourceCount<D2>(res.ParametricPool2),
               Onyx::ResourcePool_GetResourceCount<D3>(res.StaticPool3),
               Onyx::ResourcePool_GetResourceCount<D3>(res.ParametricPool3));

    cat.Expect("Default font and sampler are valid",
               Onyx::IsResourceValid(res.Font, Onyx::Resource_Font) &&
                   Onyx::IsResourceValid(res.Sampler, Onyx::Resource_Sampler),
               "The overlay and all text rendering use the default font. Textures without an explicit sampler use "
               "the default sampler",
               "font handle {}, sampler handle {}", u64(res.Font), u64(res.Sampler));

    u32 missing = 0;
    Onyx::CodePoint firstMissing = 0;
    for (Onyx::CodePoint c = 33; c < 127; ++c)
        if (!Onyx::Font_HasGlyph(res.Font, c))
        {
            if (missing++ == 0)
                firstMissing = c;
        }
    cat.Expect("Font covers printable ASCII", missing == 0,
               "DefaultResourcesOptions loads the ASCII block into the default font. Every printable character must "
               "have a glyph (space has no outline, so it is skipped)",
               "{} missing{}", missing, missing ? Format(", first one is '{}'", char(firstMissing)).Data : "");

    const Onyx::FontInfo &finfo = Onyx::Font_GetInfo(res.Font);
    cat.Expect("Font metrics are sane", finfo.LineHeight > 0.f && finfo.Ascender > finfo.Descender,
               "Line height must be positive and the ascender must be above the descender, or text layout breaks",
               "line height {:.3f}, ascender {:.3f}, descender {:.3f}, {} glyphs in pool", finfo.LineHeight,
               finfo.Ascender, finfo.Descender, Onyx::FontPool_GetGlyphCount(res.FontPool));

    const Onyx::Resource glyph = Onyx::Font_GetGlyph(res.Font, 'A');
    cat.Expect("Glyphs point back to their font", Onyx::Glyph_GetFont(glyph) == res.Font,
               "A glyph handle obtained from a font must report that same font as its owner",
               "glyph 'A' -> font {}, expected {}", u64(Onyx::Glyph_GetFont(glyph)), u64(res.Font));

    const Onyx::MaterialData<D2> &probe = Onyx::Material_GetData<D2>(app.ProbeMaterial);
    cat.Expect("Registered material reads back unchanged",
               probe.ColorFactor == Theme::Accent && near(probe.TexOffset, f32v2{0.25f}, Epsilon) &&
                   near(probe.TexScale, f32v2{2.f}, Epsilon) && !probe.Occluder,
               "Material_Register stores the data it is given. Reading it back must return exactly the same values",
               "color factor #{}, tex offset ({:.2f}, {:.2f}), tex scale ({:.2f}, {:.2f})",
               probe.ColorFactor.ToHexadecimal<TKit::StaticString<16>>().GetData(), probe.TexOffset[0],
               probe.TexOffset[1], probe.TexScale[0], probe.TexScale[1]);

    cat.Expect("Materials are typed resources",
               Onyx::IsResourceValid<D2>(app.Lit2, Onyx::Resource_Material) &&
                   Onyx::IsResourceValid<D2>(app.Occluder2, Onyx::Resource_Material) &&
                   Onyx::IsResourceValid<D3>(app.Mat3, Onyx::Resource_Material),
               "Material handles must validate as materials of their own dimension",
               "2 materials in 2D, 1 in 3D (plus the probe)");
}

void checkToolkit(App &app)
{
    Category &cat = app.Get(Category_Toolkit);

    // tensors
    const f32q q = Math::Normalize(f32q::FromEulerAngles(f32v3{0.3f, -0.7f, 1.1f}));
    const f32m4 m = Onyx::Transform<D3>::ComputeTransform(f32v3{1.f, 2.f, 3.f}, f32v3{2.f, 0.5f, 1.5f}, q);
    const f32m4 inv = Math::Inverse(m);
    cat.Expect("Matrix times its inverse is the identity", Math::Approximately(m * inv, f32m4::Identity(), 1e-4f),
               "TKit::Math::Inverse of an invertible 4x4 transform, multiplied by the original, gives the identity",
               "det(M) = {:.4f}", Math::Determinant(m));
    cat.Expect("Transposing twice is a no op", Math::Approximately(Math::Transpose(Math::Transpose(m)), m, Epsilon),
               "Transpose(Transpose(M)) = M", "4x4 transform");
    cat.Expect(
        "Inverse transform matches the matrix inverse",
        Math::Approximately(
            Onyx::Transform<D3>::ComputeInverseTransform(f32v3{1.f, 2.f, 3.f}, f32v3{2.f, 0.5f, 1.5f}, q), inv, 1e-4f),
        "Transform::ComputeInverseTransform builds the inverse directly from translation, scale and rotation. "
        "It must agree with inverting the full matrix",
        "3D, non uniform scale");
    {
        const f32m3 m2 = Onyx::Transform<D2>::ComputeTransform(f32v2{-1.f, 4.f}, f32v2{3.f, 0.25f}, 2.1f);
        const f32m3 i2 = Onyx::Transform<D2>::ComputeInverseTransform(f32v2{-1.f, 4.f}, f32v2{3.f, 0.25f}, 2.1f);
        cat.Expect("2D transform times its inverse is the identity",
                   Math::Approximately(m2 * i2, f32m3::Identity(), 1e-4f),
                   "Same as above, for the 3x3 matrices used in 2D", "2D, non uniform scale");
    }

    // quaternions
    const f32v3 v{0.4f, -1.3f, 2.2f};
    const f32v3 rotated = q * v;
    const f32v3 restored = Math::Inverse(q) * rotated;
    cat.Expect("Rotations preserve length", near(Math::Norm(rotated), Math::Norm(v), 1e-4f),
               "Rotating a vector with a unit quaternion never changes its length", "|v| = {:.5f}, |q v| = {:.5f}",
               Math::Norm(v), Math::Norm(rotated));
    cat.Expect("Inverse rotation undoes the rotation", Math::Approximately(restored, v, 1e-4f),
               "Applying q and then its inverse returns the original vector",
               "v = ({:.3f}, {:.3f}, {:.3f}), back = ({:.3f}, {:.3f}, {:.3f})", v[0], v[1], v[2], restored[0],
               restored[1], restored[2]);
    {
        const f32 angle = 1.234f;
        const f32q aa = f32q::FromAngleAxis(angle, Math::Normalize(f32v3{1.f, 2.f, -0.5f}));
        cat.Expect("Angle axis round trip", near(Math::GetAngle(aa), angle, 1e-4f),
                   "A quaternion built from an angle and an axis reports that same angle back",
                   "angle {:.4f} -> {:.4f}", angle, Math::GetAngle(aa));
    }
    cat.Expect("Degrees and radians round trip", near(Math::Degrees(Math::Radians(37.5f)), 37.5f, 1e-4f),
               "Degrees(Radians(x)) = x", "37.5 -> {:.5f}", Math::Degrees(Math::Radians(37.5f)));

    // colors
    u32 packMismatches = 0;
    for (u32 i = 0; i < 256; ++i)
    {
        const Onyx::Color c{i, 255u - i, (i * 7u) % 256u, (i * 13u) % 256u};
        packMismatches += Onyx::Color::Unpack(c.Pack()) != c;
    }
    cat.Expect("Color pack and unpack round trip", packMismatches == 0,
               "Every 8 bit channel value must survive Pack() and Unpack() exactly", "{} mismatches over 256 colors",
               packMismatches);
    {
        const Onyx::Color c{0.8f, 0.35f, 0.1f, 0.9f};
        const Onyx::Color hsv = Onyx::Color::FromHSV(c.ToHSV());
        const Onyx::Color srgb = c.ToLinear().ToSrgb();
        const Onyx::Color hex = Onyx::Color::FromHexadecimal(Onyx::Color_Lavender.ToHexadecimal<u32>());
        cat.Expect("Color conversions round trip",
                   Math::Approximately(hsv.rgba, c.rgba, 1e-4f) && Math::Approximately(srgb.rgba, c.rgba, 1e-4f) &&
                       hex == Onyx::Color_Lavender,
                   "RGB -> HSV -> RGB, sRGB -> linear -> sRGB and RGBA -> hex -> RGBA must all return the original "
                   "color",
                   "hsv error {:.6f}, srgb error {:.6f}, hex {}", Math::Norm(hsv.rgba - c.rgba),
                   Math::Norm(srgb.rgba - c.rgba), hex == Onyx::Color_Lavender ? "exact" : "different");
    }
    {
        const Onyx::Gradient gradient{TKit::Span<const Onyx::Color>{Theme::FrameTimeStops, 3}};
        cat.Expect("Gradient endpoints are exact",
                   gradient.Evaluate(0.f) == Theme::Pass && gradient.Evaluate(1.f) == Theme::Fail &&
                       Math::Approximately(gradient.Evaluate(0.5f).rgba, Theme::Todo.rgba, 1e-4f),
                   "A gradient evaluated at 0, 0.5 and 1 returns its first, middle and last stops",
                   "3 stops: green, yellow, red");
    }

    // time
    cat.Expect("Timespan conversions", near(TKit::Timespan::FromMilliseconds(1500).AsSeconds(), 1.5f, Epsilon),
               "1500 ms is 1.5 s", "FromMilliseconds(1500).AsSeconds() = {:.4f}",
               TKit::Timespan::FromMilliseconds(1500).AsSeconds());
    const f32 reading = app.MonotonicClock.GetElapsed().AsSeconds();
    app.ClockWentBackwards |= reading < app.LastClockReading;
    app.LastClockReading = reading;
    cat.Expect("Clock is monotonic", !app.ClockWentBackwards,
               "TKit::Clock::GetElapsed() must never decrease between frames", "{:.3f}s since start", reading);
    {
        u32 worst = 0;
        for (const u32 hz : {30u, 60u, 120u, 144u, 165u, 240u})
            worst = Math::Max(worst, u32(Math::Absolute(i32(Onyx::ToFrequency(Onyx::ToDeltaTime(hz))) - i32(hz))));
        cat.Expect("Frequency and delta time round trip", worst <= 1,
                   "Onyx::ToFrequency(Onyx::ToDeltaTime(hz)) returns hz again, within 1 Hz of float rounding",
                   "worst error {} Hz over 30, 60, 120, 144, 165 and 240 Hz", worst);
    }

    // containers
    {
        TKit::StaticArray<u32, 8> array{};
        for (u32 i = 0; i < 5; ++i)
            array.Append(i * 10);
        array.RemoveOrdered(array.begin() + 1);
        const bool ordered = array.GetSize() == 4 && array[0] == 0 && array[1] == 20 && array[3] == 40;
        array.RemoveUnordered(array.begin());
        const bool unordered = array.GetSize() == 3 && !array.Contains(0u) && array.Contains(40u);
        cat.Expect(
            "StaticArray removals", ordered && unordered,
            "RemoveOrdered keeps the order of the remaining elements. RemoveUnordered may reorder them, but must "
            "remove exactly the element asked for",
            "ordered {}, unordered {}", ordered ? "ok" : "wrong", unordered ? "ok" : "wrong");
    }
    {
        TKit::StaticHashMap16<u32, f32> map{};
        map.Insert(7u, 1.5f);
        map.Insert(42u, 2.5f);
        map.Insert(1000u, -1.f);
        const bool ok =
            map.Contains(7u) && map.Contains(42u) && !map.Contains(3u) && map.At(42u) == 2.5f && map.At(1000u) == -1.f;
        cat.Expect("HashMap insert and lookup", ok,
                   "Inserted keys are found with their values, keys never inserted are not", "3 keys inserted");
    }
}

void checkPlatform(App &app)
{
    Category &cat = app.Get(Category_Platform);
    Onyx::Window *win = app.Window;
    const Graph::Window handle = win->GetHandle();

    const u32 monitors = Graph::Monitor_GetCount();
    cat.Expect("At least one monitor", monitors > 0 && Graph::Monitor_IsHandleValid(Graph::Monitor_GetPrimary()),
               "The platform must report at least one monitor and a valid primary monitor", "{} monitors", monitors);

    bool monitorsOk = true;
    for (u32 i = 0; i < monitors; ++i)
    {
        const Graph::Monitor monitor = Graph::Monitor_Get(i);
        const f32v2 scale = Graph::Monitor_GetContentScale(monitor);
        monitorsOk &= scale[0] > 0.f && scale[1] > 0.f && Graph::Monitor_GetVideoMode(monitor).RefreshRate > 0;
    }
    cat.Expect("Monitors report scale and refresh rate", monitorsOk,
               "Every monitor must have a positive content scale and refresh rate. Onyx derives the default frame "
               "pacing from the refresh rate",
               "monitor delta time {:.3f} ms", win->GetMonitorDeltaTime().AsMilliseconds());

    cat.Expect("Window handle round trip",
               Graph::Window_IsHandleValid(handle) && Onyx::Window::FromHandle(handle) == win,
               "The Graphite window stores its Onyx window as user data. FromHandle(GetHandle()) must return the same "
               "window",
               "handle valid: {}", Graph::Window_IsHandleValid(handle));

    const char *title = win->GetTitle();
    bool titleOk = title != nullptr;
    for (u32 i = 0; titleOk && (app.Title[i] != '\0' || title[i] != '\0'); ++i)
        titleOk = app.Title[i] == title[i];
    cat.Expect("Title reads back", titleOk, "The title set with SetTitle must be returned by GetTitle", "\"{}\"",
               title ? title : "(null)");

    cat.Expect("Present mode reads back", u32(win->GetPresentMode()) == app.PresentMode,
               "SetPresentMode schedules a swapchain recreation, and GetPresentMode returns the requested mode",
               "requested {}, active {}", app.PresentMode, u32(win->GetPresentMode()));

    const Graph::Swapchain sc = win->GetSwapchain();
    cat.Expect("Swapchain is consistent",
               Graph::Swapchain_IsHandleValid(sc) &&
                   win->GetSwapchainImageCount() == Graph::Swapchain_GetImageCount(sc) &&
                   Graph::Swapchain_GetFormat(sc) == Onyx::SurfaceFormat,
               "The window's swapchain is valid, reports the same image count through Onyx and Graphite, and uses "
               "Onyx's surface format (B8G8R8A8_UNORM)",
               "{} images, {} recreations since start", win->GetSwapchainImageCount(), app.SwapchainRecreations);

    cat.Expect("Delta time is positive", app.DeltaTime > 0.f,
               "Every frame takes some time. A zero or negative delta time breaks camera movement and animations",
               "{:.3f} ms", 1000.f * app.DeltaTime);
}

void checkOverlay(App &app)
{
    Category &cat = app.Get(Category_Overlay);
    Onyx::Window *win = app.Window;
    const Onyx::RenderView<D2> *view = app.Ui->GetMainNativeWindow()->View;
    const f32v2 wsize = f32v2{win->GetSize()};

    bool onTop = true;
    for (const Onyx::RenderView<D2> *rv : win->GetRenderViews<D2>())
        onTop &= rv == view || rv->Layer < view->Layer;
    for (const Onyx::RenderView<D3> *rv : win->GetRenderViews<D3>())
        onTop &= rv->Layer < view->Layer;
    cat.Expect("Overlay view is the top layer", onTop,
               "The overlay draws through its own 2D render view. It must have the highest layer of all views in the "
               "window, or scene views would be drawn over it. This demo calls BringToTop on it after creating "
               "everything else",
               "overlay layer {}", view->Layer);

    const Onyx::Viewport &vp = view->GetViewport();
    cat.Expect("Overlay view covers the window", near(vp.Position, f32v2{0.f}, 0.5f) && near(vp.Size, wsize, 0.5f),
               "The overlay's view spans the whole window, so overlay windows can be placed anywhere on it",
               "viewport ({:.1f}, {:.1f}) {:.1f} x {:.1f}, window {:.0f} x {:.0f}", vp.Position[0], vp.Position[1],
               vp.Size[0], vp.Size[1], wsize[0], wsize[1]);

    cat.Expect("Playground slider stays in range", app.PlaygroundSlider >= 0.f && app.PlaygroundSlider <= 1.f,
               "A slider can only produce values inside its [min, max] range, even when typed in", "value {:.3f}",
               app.PlaygroundSlider);
}

void runChecks(App &app)
{
    for (Category &cat : app.Categories)
        cat.BeginFrame();

    checkDpi(app);
    checkViews(app);
    checkRendering(app);
    checkTextures(app);
    checkInput(app);
    checkResources(app);
    checkToolkit(app);
    checkPlatform(app);
    checkOverlay(app);

    // collect new failures into the log, oldest first
    for (const Category &cat : app.Categories)
        for (const u32 index : cat.NewFailures)
        {
            if (app.FailureLog.IsFull())
                app.FailureLog.RemoveOrdered(app.FailureLog.begin());
            const AutoCheck &check = cat.Checks[index];
            app.FailureLog.Append(FailureRecord{app.Time, cat.Title, check.Name, check.Detail});
        }
}

// =====================================================================================================================
//  8. SCENE DRAWING
// =====================================================================================================================

// fine pattern in logical pixels. "align" puts integer coordinates on pixel centers, so 1 px features land exactly on
// one pixel instead of straddling two
void drawPixelPattern(Onyx::RenderContext<D2> *ctx, const f32 halfExtent, const f32v2 &align)
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
f32v2 pixelAlignment(const Onyx::RenderView<D2> *rv)
{
    const f32v2 corner = rv->ScreenToWorld(rv->GetViewport().Position);
    return corner - f32v2{std::floor(corner[0]), std::floor(corner[1])} + 0.5f;
}

// brackets just inside the four viewport corners. if one is missing, the viewport or scissor is wrong
void drawCornerBrackets(Onyx::RenderContext<D2> *ctx, const Onyx::RenderView<D2> *rv, const f32 inset)
{
    const Onyx::Viewport &vp = rv->GetViewport();
    const f32v2 corners[4] = {vp.Position + inset, vp.Position + f32v2{vp.Size[0] - inset, inset},
                              vp.Position + f32v2{inset, vp.Size[1] - inset}, vp.Position + vp.Size - inset};
    const f32v2 center = vp.Position + 0.5f * vp.Size;
    const f32 pixel = Math::Norm(rv->ScreenToWorld(center + f32v2{1.f, 0.f}) - rv->ScreenToWorld(center));
    const f32 arm = 14.f;

    ctx->Push();
    ctx->FillColor(Theme::Corner);
    for (const f32v2 &c : corners)
    {
        const f32v2 towardsX = f32v2{c[0] < center[0] ? arm : -arm, 0.f};
        const f32v2 towardsY = f32v2{0.f, c[1] < center[1] ? arm : -arm};
        const f32v2 w = rv->ScreenToWorld(c);
        ctx->Line(w, rv->ScreenToWorld(c + towardsX), 2.f * pixel);
        ctx->Line(w, rv->ScreenToWorld(c + towardsY), 2.f * pixel);
    }
    ctx->Pop();
}

void drawCursor(Onyx::RenderContext<D2> *ctx, const Onyx::RenderView<D2> *rv, const f32v2 &mouse, const f32 radiusPx)
{
    if (!rv->IsWithinViewport(mouse))
        return;
    const f32v2 center = rv->ScreenToWorld(mouse);
    const f32 pixel = Math::Norm(rv->ScreenToWorld(mouse + f32v2{1.f, 0.f}) - center);
    ctx->Push();
    ctx->FillColor(Theme::Cursor);
    ctx->Scale(2.f * radiusPx * pixel);
    ctx->Translate(center);
    ctx->Circle({.Hollowness = 0.75f});
    ctx->Pop();
}

void drawTitle(Onyx::RenderContext<D2> *ctx, const Onyx::RenderView<D2> *rv, const char *text, const f32 sizePx)
{
    const Onyx::Viewport &vp = rv->GetViewport();
    const f32v2 anchor = vp.Position + f32v2{0.5f * vp.Size[0], 26.f};
    const f32 pixel = Math::Norm(rv->ScreenToWorld(anchor + f32v2{1.f, 0.f}) - rv->ScreenToWorld(anchor));
    ctx->Push();
    ctx->FillColor(Theme::Text);
    ctx->Align(Onyx::Alignment_Center);
    ctx->Scale(sizePx * pixel);
    ctx->Translate(rv->ScreenToWorld(anchor));
    ctx->Text(text);
    ctx->Pop();
}

void drawPixelView(App &app, const f32v2 &mouse)
{
    Onyx::RenderContext<D2> *ctx = app.PixelCtx;
    ctx->Flush();
    const f32v2 align = pixelAlignment(app.PixelView);

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

    drawTitle(ctx, app.PixelView, "PIXEL VIEW  1 unit = 1 logical px", 15.f);
    drawCornerBrackets(ctx, app.PixelView, 3.f);
    drawCursor(ctx, app.PixelView, mouse, 10.f);
}

void drawGallery2(App &app, const f32v2 &mouse)
{
    Onyx::RenderContext<D2> *ctx = app.GalleryCtx;
    const RenderSettings &rs = app.Render;
    ctx->Flush();

    // background grid, one line per world unit
    ctx->FillColor(Theme::Grid);
    for (i32 i = -15; i <= 15; ++i)
    {
        ctx->Line(f32v2{f32(i), -15.f}, f32v2{f32(i), 15.f}, 0.02f);
        ctx->Line(f32v2{-15.f, f32(i)}, f32v2{15.f, f32(i)}, 0.02f);
    }

    // lights: a dark receiver band, a light following the cursor and a spinning occluder casting a shadow
    if (rs.Lights2)
    {
        app.LightPos2 = app.GalleryView->IsWithinViewport(mouse)
                            ? app.GalleryView->ScreenToWorld(mouse)
                            : f32v2{4.f * Math::Cosine(app.Time), -3.4f + 0.8f * Math::Sine(app.Time)};

        // lights are transformed by the current context transform, like any draw. emit it before touching the
        // transform, so its position stays in world space
        ctx->PointLight({.Position = app.LightPos2,
                         .Tint = Onyx::Color_Honey,
                         .LightRadius = 4.f,
                         .ShadowRadius = 8.f,
                         .Intensity = 1.2f,
                         .Flags = Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF});

        ctx->Push();
        ctx->RenderFlags(Onyx::RenderModeFlag_Shaded);
        ctx->Material(app.Lit2);
        ctx->FillColor(Onyx::Color{70u, 74u, 96u});
        ctx->Scale(f32v2{14.f, 2.6f});
        ctx->Translate(f32v2{0.f, -3.4f});
        ctx->Quad();

        ctx->Material(app.Occluder2);
        ctx->FillColor(Onyx::Color_Rose);
        ctx->ResetTransform();
        ctx->Scale(0.7f);
        ctx->Rotate(app.Time);
        ctx->Translate(f32v2{0.f, -3.4f});
        ctx->Quad();
        ctx->Pop();
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
        ctx->Scale(1.4f);
        if (rs.Rotate && i != 2)
            ctx->Rotate(0.5f * app.Time + f32(i));
        ctx->Translate(f32v2{-4.4f + 2.2f * f32(i), 2.4f});
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

    // middle row: three overlapping translucent squares
    ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
    for (u32 i = 0; i < 3; ++i)
    {
        ctx->Push();
        ctx->FillColor(Theme::Shapes[i + 3]);
        ctx->Alpha(rs.Alpha);
        ctx->Scale(1.6f);
        ctx->Translate(f32v2{-0.7f + 0.7f * f32(i), -0.2f + 0.35f * f32(i % 2)});
        ctx->Quad();
        ctx->Pop();
    }

    // lines and text
    ctx->Push();
    ctx->FillColor(Onyx::Color_Mint);
    ctx->RoundedLine(f32v2{-5.2f, -0.6f}, f32v2{-2.6f, 0.4f}, 0.15f);
    ctx->FillColor(Onyx::Color_Peach);
    ctx->Line(f32v2{2.6f, 0.4f}, f32v2{5.2f, -0.6f}, 0.15f);
    ctx->Pop();

    drawTitle(ctx, app.GalleryView, "2D GALLERY", 15.f);
    drawCornerBrackets(ctx, app.GalleryView, 3.f);
    drawCursor(ctx, app.GalleryView, mouse, 10.f);
}

void drawGallery3(App &app)
{
    Onyx::RenderContext<D3> *ctx = app.Ctx3;
    const RenderSettings &rs = app.Render;
    ctx->Flush();

    Onyx::RenderModeFlags flags = Onyx::RenderModeFlag_Shaded;
    if (rs.Outlines)
        flags |= Onyx::RenderModeFlag_Outlined;
    ctx->RenderFlags(flags);
    ctx->OutlineColor(Theme::Text);
    ctx->OutlineWidth(rs.OutlineWidth);
    ctx->Material(app.Mat3);

    if (rs.Lights3)
    {
        ctx->DirectionalLight({.Direction = f32v3{-0.6f, -1.f, -0.4f},
                               .Cascades = {.View = app.View3},
                               .Intensity = 0.7f,
                               .Flags = Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF});
        if (rs.PointLight3)
        {
            // ShadowRadius is the far plane of the cube shadow map (default 4). shapes here can be ~10 units away
            // from the light, so it must be larger. the near plane is 5% of it, so do not overdo it either
            const f32v3 lightPos{3.5f * Math::Cosine(app.Time), 2.5f, 3.5f * Math::Sine(app.Time)};
            ctx->PointLight({.Position = lightPos,
                             .Tint = Onyx::Color_Honey,
                             .LightRadius = 6.f,
                             .ShadowRadius = 14.f,
                             .Intensity = 1.6f,
                             .Flags = Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF});

            // a small unlit sphere marks where the light is
            ctx->Push();
            ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
            ctx->FillColor(Onyx::Color_Buttercup);
            ctx->Scale(0.25f);
            ctx->Translate(lightPos);
            ctx->Sphere();
            ctx->Pop();
        }
        if (rs.SpotLight3)
            ctx->SpotLight({.Position = f32v3{0.f, 4.f, 2.f},
                            .Direction = Math::Normalize(f32v3{0.4f * Math::Sine(0.7f * app.Time), -1.f, -0.5f}),
                            .Tint = Onyx::Color_Sky,
                            .FieldOfView = Math::Radians(40.f),
                            .LightRange = 8.f,
                            .Intensity = 1.5f,
                            .Flags = Onyx::LightFlag_CastShadows});
    }

    // floor
    ctx->Push();
    ctx->RemoveRenderFlags(Onyx::RenderModeFlag_Outlined);
    ctx->FillColor(Onyx::Color{90u, 92u, 110u});
    ctx->ScaleX(24.f);
    ctx->ScaleY(24.f);
    ctx->RotateX(-0.5f * Math::Pi<f32>());
    ctx->TranslateY(-1.6f);
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
}

void drawTextures(App &app, const f32v2 &mouse)
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
    rt->Text("render texture");
    rt->Pop();

    Onyx::RenderContext<D2> *ctx = app.TexturesCtx;
    ctx->Flush();
    const f32v2 align = pixelAlignment(app.TexturesView);
    const f32 size = f32(RenderTextureSize[0]);

    // drawn at their logical size, so the scaled one maps one texel to one physical pixel
    ctx->Push();
    ctx->Scale(size);
    ctx->Translate(f32v2{-100.f, 0.f} + align - 0.5f);
    ctx->Image(*app.RtScaled);
    ctx->Pop();

    ctx->Push();
    ctx->Scale(size);
    ctx->Translate(f32v2{100.f, 0.f} + align - 0.5f);
    ctx->Image(*app.RtFixed);
    ctx->Pop();

    const auto label = [&](const char *text, const f32 x) {
        ctx->Push();
        ctx->FillColor(Theme::Dim);
        ctx->Align(Onyx::Alignment_Center);
        ctx->Scale(13.f);
        ctx->Translate(f32v2{x, -100.f});
        ctx->Text(text);
        ctx->Pop();
    };
    label("follows window scale", -100.f);
    label("fixed at scale 1", 100.f);

    drawTitle(ctx, app.TexturesView, "TEXTURES  offset viewport", 15.f);
    drawCornerBrackets(ctx, app.TexturesView, 3.f);
    drawCursor(ctx, app.TexturesView, mouse, 10.f);
}

// the 3D gallery's title, brackets and cursor are drawn by a transparent 2D view stacked on top of it
void drawGallery3Labels(App &app, const f32v2 &mouse)
{
    Onyx::RenderContext<D2> *ctx = app.LabelCtx;
    ctx->Flush();
    drawTitle(ctx, app.Label3, "3D GALLERY", 15.f);
    drawCornerBrackets(ctx, app.Label3, 3.f);
    drawCursor(ctx, app.Label3, mouse, 10.f);
}

void drawScene(App &app)
{
    const f32v2 mouse = app.Window->GetMousePosition();
    drawPixelView(app, mouse);
    drawGallery2(app, mouse);
    drawGallery3(app);
    drawGallery3Labels(app, mouse);
    drawTextures(app, mouse);
}

// =====================================================================================================================
//  9. OVERLAY PANELS
// =====================================================================================================================

bool beginPanel(App &app, const CategoryId id)
{
    Category &cat = app.Get(id);
    if (!cat.Open)
        return false;
    const f32 offset = 28.f * f32(id);
    app.Ui->SetNextWindowPosition(f32v2{470.f + offset, 20.f + offset}, Onyx::OverlayCondition_OnBirth);
    app.Ui->SetNextWindowSize(f32v2{520.f, 760.f}, Onyx::OverlayCondition_OnBirth);
    return app.Ui->BeginWindow(cat.Title, &cat.Open);
}

void endPanel(App &app, const CategoryId id)
{
    Category &cat = app.Get(id);
    drawAutoChecks(app.Ui, cat, app.ShowRules);
    drawVisualChecks(app.Ui, cat, 100u * u32(id));
    app.Ui->EndWindow();
}

void panelDpi(App &app)
{
    if (!beginPanel(app, Category_Dpi))
        return;
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;

    concepts(ui, "Concepts: logical and physical pixels",
             "A physical pixel is a dot on the screen. A logical pixel is what your layout is written in. The content "
             "scale is the ratio between them: at 1.5x, a 100 x 100 logical rectangle covers 150 x 150 physical "
             "pixels. Onyx works in logical pixels everywhere (window size, viewports, mouse, overlay layout) and only "
             "multiplies by the content scale where real pixels are allocated: swapchain images, view framebuffers and "
             "render textures. A DPI bug is almost always a place where the two spaces get mixed: blurry output "
             "(rendering at logical resolution on a physical framebuffer), clipped output (physical sizes used as "
             "logical) or a drifting cursor (mouse in one space, viewport in the other).");
    concepts(ui, "Concepts: screen coordinates",
             "GLFW's screen coordinates are logical on Wayland and macOS, and physical on X11 and Windows. Onyx hides "
             "that: Window::GetSize() and GetMousePosition() always return logical pixels, derived from the "
             "framebuffer size divided by the content scale.");

    const Graph::Window handle = win->GetHandle();
    const f32v2 scale = win->GetContentScale();
    const u32v2 size = win->GetSize();
    const u32v2 screen = Graph::Window_GetScreenSize(handle);
    const u32v2 pixels = Graph::Window_GetPixelSize(handle);

    ui->HorizontalSeparator("Measured");
    info(ui, "Content scale", "{:.3f} x {:.3f}", scale[0], scale[1]);
    info(ui, "Logical size", "{} x {}", size[0], size[1]);
    info(ui, "Screen size (platform units)", "{} x {}", screen[0], screen[1]);
    info(ui, "Framebuffer size (physical)", "{} x {}", pixels[0], pixels[1]);
    info(ui, "Swapchain recreations", "{}", app.SwapchainRecreations);
    const Graph::Monitor monitor = win->GetMonitor();
    // graphite's null handle is all ones, not zero, so a plain truthiness test does not work here
    if (Graph::Monitor_IsHandleValid(monitor))
    {
        const f32v2 ms = Graph::Monitor_GetContentScale(monitor);
        info(ui, "Monitor scale (fullscreen only)", "{:.3f} x {:.3f}", ms[0], ms[1]);
    }

    ui->HorizontalSeparator("Content scale events");
    if (app.ScaleEvents.IsEmpty())
        explain(ui, "None yet. Change the monitor scale (or drag the window to a monitor with a different scale) and "
                    "every change must show up here.");
    for (const ScaleEvent &e : app.ScaleEvents)
        colored(ui, Theme::Info, "t = {:7.2f}s  ->  {:.3f} x {:.3f}", e.Time, e.Scale[0], e.Scale[1]);

    endPanel(app, Category_Dpi);
}

void panelViews(App &app)
{
    if (!beginPanel(app, Category_Views))
        return;
    Onyx::Overlay *ui = app.Ui;

    concepts(ui, "Concepts: render targets, views and cameras",
             "A render target is something that can be rendered to: a window or a render texture. A render view is a "
             "rectangle (viewport) inside a target, with its own framebuffers, camera and passes. A camera decides "
             "what part of the world the view shows: CameraMode_Orthographic shows OrthoParameters.Size world units "
             "vertically, CameraMode_Viewport maps one world unit to one logical pixel, CameraMode_Perspective (3D "
             "only) uses a field of view. A render context records draws and can target several views at once.");
    concepts(ui, "Concepts: sizing policies and layers",
             "Viewport, scissor and image size can each be proportional (fractions of the parent, recomputed on "
             "resize) or fixed (absolute logical pixels). Views are drawn in increasing Layer order: BringToTop and "
             "BringToBottom move a view to either end. Coordinates: screen = logical pixels from the window's top "
             "left, viewport = the same relative to the view, world = what the camera sees.");

    ui->HorizontalSeparator("Live values");
    const f32v2 mouse = app.Window->GetMousePosition();
    info(ui, "Mouse (screen)", "({:.1f}, {:.1f})", mouse[0], mouse[1]);
    const auto viewRow = [&](const char *name, const Onyx::RenderView<D2> *rv) {
        const Onyx::Viewport &vp = rv->GetViewport();
        const u32v2 ext = rv->GetImageExtent();
        colored(ui, Theme::Text, "{}", name);
        colored(ui, Theme::Info, "    viewport ({:.0f}, {:.0f}) {:.0f} x {:.0f}   extent {} x {}   layer {}",
                vp.Position[0], vp.Position[1], vp.Size[0], vp.Size[1], ext[0], ext[1], rv->Layer);
        if (rv->IsWithinViewport(mouse))
        {
            const f32v2 w = rv->ScreenToWorld(mouse);
            colored(ui, Theme::Cursor, "    mouse in world ({:.3f}, {:.3f})", w[0], w[1]);
        }
    };
    viewRow("Pixel view", app.PixelView);
    viewRow("2D gallery", app.GalleryView);
    viewRow("Textures", app.TexturesView);
    {
        const Onyx::Viewport &vp = app.View3->GetViewport();
        const f32v3 &pos = app.Cam3.View.Translation;
        colored(ui, Theme::Text, "3D gallery");
        colored(ui, Theme::Info,
                "    viewport ({:.0f}, {:.0f}) {:.0f} x {:.0f}   camera ({:.2f}, {:.2f}, {:.2f})   fov {:.1f} deg",
                vp.Position[0], vp.Position[1], vp.Size[0], vp.Size[1], pos[0], pos[1], pos[2],
                Math::Degrees(app.Cam3.PerspParameters.FieldOfView));
    }

    ui->HorizontalSeparator("Controls");
    ui->HorizontalSlider("2D ortho size", &app.GalleryCam.OrthoParameters.Size, 1.f, 40.f);
    if (ui->Button("Reset cameras"))
        resetCameras(app);

    endPanel(app, Category_Views);
}

void panelRendering(App &app)
{
    if (!beginPanel(app, Category_Rendering))
        return;
    Onyx::Overlay *ui = app.Ui;
    RenderSettings &rs = app.Render;

    concepts(ui, "Concepts: geometry and render modes",
             "Onyx draws five kinds of geometry: circles (analytic, with hollowness, arcs and fades), static meshes "
             "(triangle, quad, box...), parametric meshes (stadium, rounded rect, capsule, torus: one mesh, shape set "
             "per instance), glyphs (text) and dynamic meshes (your own vertices). Each draw is flat (unlit color), "
             "shaded (lit by the context's lights, using its material) and optionally outlined. Outlines are drawn in "
             "the post process pass, so the view needs RenderViewFlag_Outlines and RenderViewFlag_PostProcess.");
    concepts(ui, "Concepts: passes",
             "Opaque draws go first and write depth. Translucent draws (alpha below 1) go to the transparent pass and "
             "are blended on top, if the view has RenderViewFlag_Transparency. Shadow maps are rendered for lights "
             "with LightFlag_CastShadows when the view has RenderViewFlag_Shadows. In 2D, occluder materials block "
             "light; in 3D every shaded mesh casts and receives.");

    ui->HorizontalSeparator("Controls");
    ui->CheckBox("Outlines", &rs.Outlines);
    ui->HorizontalSlider("Outline width", &rs.OutlineWidth, 0.f, 0.3f);
    ui->CheckBox("Rotate shapes", &rs.Rotate);
    ui->HorizontalSlider("Alpha (middle row)", &rs.Alpha, 0.05f, 1.f);
    ui->HorizontalSlider("Circle hollowness", &rs.Circle.Hollowness, 0.f, 0.95f);
    ui->HorizontalSlider("Circle arc end", &rs.Circle.UpperAngle, 0.1f, 2.f * Math::Pi<f32>());
    ui->HorizontalSlider("Circle inner fade", &rs.Circle.InnerFade, 0.f, 1.f);
    ui->HorizontalSlider("Circle outer fade", &rs.Circle.OuterFade, 0.f, 1.f);
    ui->CheckBox("2D lights", &rs.Lights2);
    ui->CheckBox("3D lights", &rs.Lights3);
    ui->CheckBox("3D point light", &rs.PointLight3);
    ui->CheckBox("3D spot light", &rs.SpotLight3);

    // view flags: changing them goes through RenderView::SetFlags
    const bool oldShadows = rs.Shadows;
    const bool oldPost = rs.PostProcess;
    const bool oldOutlines = rs.Outlines;
    ui->CheckBox("View: shadows", &rs.Shadows);
    ui->CheckBox("View: post process", &rs.PostProcess);
    if (oldShadows != rs.Shadows || oldPost != rs.PostProcess || oldOutlines != rs.Outlines)
    {
        Onyx::RenderViewFlags flags = Onyx::RenderViewFlag_Transparency;
        if (rs.Shadows)
            flags |= Onyx::RenderViewFlag_Shadows;
        if (rs.PostProcess)
            flags |= Onyx::RenderViewFlag_PostProcess;
        if (rs.Outlines)
            flags |= Onyx::RenderViewFlag_Outlines;
        app.GalleryView->SetFlags(flags);
        app.View3->SetFlags(flags);
    }
    if (rs.Outlines && !rs.PostProcess)
        colored(ui, Theme::Todo, "Outlines need the post process pass: expect no outlines now");

    endPanel(app, Category_Rendering);
}

void panelTextures(App &app)
{
    if (!beginPanel(app, Category_Textures))
        return;
    Onyx::Overlay *ui = app.Ui;

    concepts(ui, "Concepts: render textures",
             "A render texture is an offscreen render target. You create views on it like on a window, render into "
             "them with contexts, and then sample the result as a texture: in a context (ctx->Image) or in the "
             "overlay (ui->Image). It has its own logical size and content scale. Rendering at the window's scale "
             "gives a texture that maps one texel to one physical pixel when drawn at its logical size.");

    const f32v2 imageSize = f32v2{RenderTextureSize};
    ui->HorizontalSeparator("Sampled in the overlay");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
    ui->Image(*app.RtScaled, imageSize);
    ui->Image(*app.RtFixed, imageSize);
    ui->PopPanel();
    explain(ui, "Left: follows the window scale. Right: fixed at scale 1.");

    endPanel(app, Category_Textures);
}

void panelInput(App &app)
{
    if (!beginPanel(app, Category_Input))
        return;
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;
    const InputTracking &in = app.Input;

    concepts(ui, "Concepts: events and polling",
             "Each window collects events (keys, mouse, scroll, resize, focus, scale...) while Onyx polls the "
             "platform. Window::GetNewEvents() returns the events gathered since the previous frame. Polling "
             "(IsKeyPressed, GetMousePosition) returns the state right now. Both views of the input must agree, and "
             "the event stream must be internally consistent: a release always follows a press, focus gain and loss "
             "alternate, and so on. The overlay consumes input too: WantCaptureMouse/Keyboard tell the application "
             "when to ignore it.");

    ui->HorizontalSeparator("Live state");
    const f32v2 mouse = win->GetMousePosition();
    info(ui, "Mouse (polled)", "({:.1f}, {:.1f})", mouse[0], mouse[1]);
    if (in.HasEventMouse)
    {
        info(ui, "Mouse (last move event)", "({:.1f}, {:.1f})", in.EventMouse[0], in.EventMouse[1]);
        info(ui, "Event vs polled distance", "{:.2f} px", Math::Norm(in.EventMouse - mouse));
        explain(ui, "Should be about 0 while moving slowly. A constant offset, or a distance that grows with the "
                    "position by a factor equal to the content scale, means events and polling use different spaces.");
    }
    info(ui, "Buttons held (events)", "{}", in.HeldButtons.GetSize());
    info(ui, "Keys held (events)", "{}", in.HeldKeys.GetSize());
    info(ui, "Last key pressed", "{}", in.LastKey);
    info(ui, "Last scroll", "({:.2f}, {:.2f})", in.LastScroll[0], in.LastScroll[1]);
    info(ui, "Typed text", "\"{}\"", TKit::StringView{in.Typed, in.TypedSize});
    info(ui, "Overlay wants", "mouse {}, scroll {}, keyboard {}", ui->WantCaptureMouse(), ui->WantCaptureScroll(),
         ui->WantCaptureKeyboard());
    info(ui, "Window", "hovered {}, fullscreen {}", win->IsHovered(), win->IsFullScreen());

    ui->HorizontalSeparator("Event counters since start");
    for (u32 i = 0; i < Onyx::Event_Count; ++i)
    {
        const u32 count = in.EventCounts[i];
        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        colored(ui, count ? Theme::Info : Theme::Dim, "{:>8}", count);
        coloredRaw(ui, count ? Theme::Text : Theme::Dim, EventNames[i]);
        ui->PopPanel();
    }

    endPanel(app, Category_Input);
}

void panelResources(App &app)
{
    if (!beginPanel(app, Category_Resources))
        return;
    Onyx::Overlay *ui = app.Ui;

    concepts(ui, "Concepts: resources",
             "Meshes, fonts, materials, samplers, images and textures are referred to by handles (Onyx::Resource). "
             "Unlike raw Graphite handles, they are bound and ready to use: give a handle to a context or a material "
             "and it just works. Meshes and fonts are grouped in pools. Registering a resource only records it: "
             "Resources_Sync uploads everything pending to the GPU, so sync after a batch of registrations.");

    ui->HorizontalSeparator("Counts");
    info(ui, "Dynamic meshes (2D / 3D)", "{} / {}", Onyx::DynamicMesh_GetCount<D2>(), Onyx::DynamicMesh_GetCount<D3>());
    info(ui, "Font pool", "{} fonts, {} glyphs", Onyx::FontPool_GetFontCount(Onyx::Resources_GetDefault().FontPool),
         Onyx::FontPool_GetGlyphCount(Onyx::Resources_GetDefault().FontPool));

    endPanel(app, Category_Resources);
}

void panelToolkit(App &app)
{
    if (!beginPanel(app, Category_Toolkit))
        return;
    Onyx::Overlay *ui = app.Ui;

    concepts(ui, "Concepts: what lives in Toolkit",
             "Toolkit (TKit) is the foundation: tensors (vectors, matrices) and quaternions, containers with explicit "
             "allocation strategies (static, stack, arena, tier), timing, logging and multiprocessing. Onyx adds "
             "Transform (composition and decomposition of translation, scale and rotation) and Color on top. These "
             "checks run every frame on fixed inputs, so a failure here is a math or container regression, not a "
             "rendering issue.");

#ifdef TKIT_ENABLE_ENSURE
    if (ui->PushTree("Tier allocator statistics"))
    {
        ui->ShowTierAllocatorStatistics();
        ui->PopTree();
    }
#endif

    endPanel(app, Category_Toolkit);
}

void panelPlatform(App &app)
{
    if (!beginPanel(app, Category_Platform))
        return;
    Onyx::Overlay *ui = app.Ui;
    Onyx::Window *win = app.Window;

    concepts(ui, "Concepts: Graphite",
             "Graphite (Graph) is the layer between Onyx and the system: a platform backend (GLFW) for windows, "
             "monitors and input, and a render backend (Vulkan) for devices, swapchains, pipelines and resources. "
             "Onyx windows wrap a Graphite window and own its swapchain.");

    const Graph::RenderBackendInfo rinfo = Graph::GetRenderBackendInfo();
    const Graph::PlatformBackendInfo pinfo = Graph::GetPlatformBackendInfo();
    ui->HorizontalSeparator("Device");
    info(ui, "GPU", "{}", rinfo.Device.Name);
    info(ui, "Driver", "{} ({})", rinfo.Device.DriverName, rinfo.Device.DriverInfo);
    info(ui, "API", "{}.{}.{}", rinfo.Device.ApiMajor, rinfo.Device.ApiMinor, rinfo.Device.ApiPatch);
    info(ui, "Render backend", "{}", rinfo.Version);
    info(ui, "Platform backend", "{} on {}", pinfo.Version, platformName(pinfo.Platform));

    ui->HorizontalSeparator("Monitors");
    for (u32 i = 0; i < Graph::Monitor_GetCount(); ++i)
    {
        const Graph::Monitor monitor = Graph::Monitor_Get(i);
        const Graph::VideoMode mode = Graph::Monitor_GetVideoMode(monitor);
        const f32v2 scale = Graph::Monitor_GetContentScale(monitor);
        const Graph::MonitorArea area = Graph::Monitor_GetWorkArea(monitor);
        colored(ui, Theme::Text, "Monitor {}{}", i, monitor == Graph::Monitor_GetPrimary() ? " (primary)" : "");
        colored(ui, Theme::Info, "    {} x {} @ {} Hz, scale {:.2f}, work area {} x {} at ({}, {})", mode.Size[0],
                mode.Size[1], mode.RefreshRate, scale[0], area.Size[0], area.Size[1], area.Position[0],
                area.Position[1]);
    }

    ui->HorizontalSeparator("Window");
    const i32v2 pos = win->GetPosition();
    info(ui, "Position", "({}, {}) (not reported on Wayland)", pos[0], pos[1]);
    info(ui, "Opacity (read back)", "{:.2f}", win->GetOpacity());
    if (ui->DropDown("Present mode", &app.PresentMode, "Immediate#Mailbox#VSync"))
        win->SetPresentMode(Graph::PresentMode(app.PresentMode));
    if (ui->HorizontalSlider("Opacity", &app.Opacity, 0.2f, 1.f))
        win->SetOpacity(app.Opacity);
    explain(ui, "Changing the present mode recreates the swapchain. Opacity is not supported by every platform "
                "(Wayland ignores it), so its read back is informative only.");

    endPanel(app, Category_Platform);
}

void panelOverlay(App &app)
{
    if (!beginPanel(app, Category_Overlay))
        return;
    Onyx::Overlay *ui = app.Ui;

    concepts(ui, "Concepts: the overlay",
             "The overlay is Onyx's immediate mode UI: every frame you describe the windows and widgets, and it lays "
             "them out with the layout engine and draws them through its own render view. It supports menus, tabs, "
             "trees, popups, tooltips, drag and drop, docking (enabled in this demo with OverlayFlag_Docking) and "
             "window promotion to native windows (not on Wayland).");

    ui->HorizontalSeparator("Playground");
    if (ui->Button("Click me"))
        ++app.ButtonClicks;
    ui->SetItemTooltipRaw("A tooltip. Right click the button for a popup");
    if (ui->BeginPopupContextItem("Playground popup"))
    {
        colored(ui, Theme::Accent, "A popup menu");
        if (ui->MenuItem("Reset the click counter"))
            app.ButtonClicks = 0;
        ui->EndPopup();
    }
    colored(ui, Theme::Info, "Clicked {} times", app.ButtonClicks);
    ui->CheckBox("A checkbox", &app.PlaygroundBool);
    ui->HorizontalSlider("A slider", &app.PlaygroundSlider, 0.f, 1.f);
    ui->HorizontalDrag("A drag", &app.PlaygroundDrag, 1.f, -100, 100);
    ui->DropDown("A drop down", &app.PlaygroundChoice, "First#Second#Third");
    ui->InputText("A text field", app.PlaygroundText, sizeof(app.PlaygroundText));
    ui->ColorEditor("A color", &app.PlaygroundColor);
    ui->ProgressBar("A progress bar", app.PlaygroundSlider, "{:.0f}%", 100.f * app.PlaygroundSlider);

    ui->HorizontalSeparator("Built in tools");
    ui->CheckBox("Show the overlay demo", &app.ShowOverlayDemo);
    ui->CheckBox("Show the style editor", &app.ShowStyleEditor);

    endPanel(app, Category_Overlay);
}

void drawFrameTimes(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    const f32 target = Math::Max(app.Window->GetMonitorDeltaTime().AsSeconds(), 1e-4f);
    const Onyx::Gradient gradient{TKit::Span<const Onyx::Color>{Theme::FrameTimeStops, 3}};
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

void drawControlCenter(App &app)
{
    Onyx::Overlay *ui = app.Ui;
    ui->SetNextWindowPosition(f32v2{16.f, 16.f}, Onyx::OverlayCondition_OnBirth);
    ui->SetNextWindowSize(f32v2{440.f, 860.f}, Onyx::OverlayCondition_OnBirth);
    if (!ui->BeginWindow("Control center", Onyx::OverlayWindowFlag_MenuBar))
        return;

    if (ui->BeginMenuBar())
    {
        if (ui->BeginMenu("Panels"))
        {
            for (Category &cat : app.Categories)
                ui->MenuItem(cat.Title, &cat.Open);
            ui->EndMenu();
        }
        if (ui->BeginMenu("View"))
        {
            ui->MenuItem("Show rules under each check", &app.ShowRules);
            ui->MenuItem("Overlay demo", &app.ShowOverlayDemo);
            ui->MenuItem("Style editor", &app.ShowStyleEditor);
            ui->EndMenu();
        }
        ui->EndMenuBar();
    }

    // -- header ----------------------------------------------------------------------------------------------------
    colored(ui, Theme::Accent, "ONYX CONTROL CENTER");
    const Graph::RenderBackendInfo rinfo = Graph::GetRenderBackendInfo();
    colored(ui, Theme::Dim, "{} | {}", rinfo.Device.Name, platformName(Graph::GetPlatformBackendInfo().Platform));

    // -- totals ----------------------------------------------------------------------------------------------------
    u32 total = 0, passed = 0, visuals = 0, good = 0, broken = 0;
    for (const Category &cat : app.Categories)
    {
        total += cat.Checks.GetSize();
        passed += cat.CountPassed();
        visuals += cat.Visuals.GetSize();
        good += cat.CountVisuals(Verdict_Good);
        broken += cat.CountVisuals(Verdict_Broken);
    }
    ui->HorizontalSeparator("Summary");
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    badge(ui, passed == total ? "PASS" : "FAIL", passed == total ? Theme::Pass : Theme::Fail);
    colored(ui, Theme::Text, "{} / {} automatic checks pass", passed, total);
    ui->PopPanel();
    ui->ProgressBar("Automatic", total ? f32(passed) / f32(total) : 1.f, "{}/{}", passed, total);
    ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
    badge(ui, broken ? "BAD" : (good == visuals ? "GOOD" : "TODO"),
          broken ? Theme::Fail : (good == visuals ? Theme::Pass : Theme::Todo));
    colored(ui, Theme::Text, "{} / {} visual checks reviewed, {} broken", good + broken, visuals, broken);
    ui->PopPanel();
    ui->ProgressBar("Visual", visuals ? f32(good + broken) / f32(visuals) : 1.f, "{}/{}", good + broken, visuals);

    // -- frame timing ----------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Frame timing");
    f32 avg = 0.f, worst = 0.f;
    for (const f32 dt : app.FrameTimes)
    {
        avg += dt;
        worst = Math::Max(worst, dt);
    }
    avg /= f32(app.FrameTimes.GetSize());
    colored(ui, Theme::Info, "{:.0f} fps   avg {:.2f} ms   worst {:.2f} ms   monitor {:.2f} ms",
            avg > 0.f ? 1.f / avg : 0.f, 1000.f * avg, 1000.f * worst,
            app.Window->GetMonitorDeltaTime().AsMilliseconds());
    drawFrameTimes(app);
    explain(ui, "Bars are the last frames. Green is on the monitor's refresh interval, red is twice as slow or worse.");

    // -- categories ------------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Categories");
    explain(ui, "Tick a category to open its panel. Each panel explains the concepts behind it, lists its automatic "
                "checks with the rule that must hold, and the visual checks you have to judge yourself.");
    for (u32 i = 0; i < Category_Count; ++i)
    {
        Category &cat = app.Categories[i];
        const u32 cp = cat.CountPassed();
        const u32 cn = cat.Checks.GetSize();
        const u32 todo = cat.CountVisuals(Verdict_Unseen);
        const u32 bad = cat.CountVisuals(Verdict_Broken);

        ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
        badge(ui, cp == cn ? "PASS" : "FAIL", cp == cn ? Theme::Pass : Theme::Fail);
        if (bad)
            badge(ui, "BAD", Theme::Fail);
        else if (todo)
            badge(ui, "TODO", Theme::Todo);
        ui->CheckBox(cat.Title, &cat.Open);
        ui->PopPanel();
        colored(ui, Theme::Dim, "    {}/{} pass, {} to review, {} failures since start | {}", cp, cn, todo,
                cat.FailuresSinceStart, cat.Summary);
    }

    // -- failure log -----------------------------------------------------------------------------------------------
    ui->HorizontalSeparator("Failure log");
    explain(ui, "Every time an automatic check goes from passing to failing, it is recorded here with what was "
                "measured, so failures that last a single frame (during a resize or a scale change) are not missed.");
    if (app.FailureLog.IsEmpty())
        colored(ui, Theme::Pass, "No failures recorded");
    else
    {
        if (ui->Button("Clear log"))
            app.FailureLog.Clear();
        ui->BeginScroll("Failure log scroll", 220.f, Onyx::OverlayScrollFlag_Borders);
        for (u32 i = app.FailureLog.GetSize() - 1; i < app.FailureLog.GetSize(); --i)
        {
            const FailureRecord &record = app.FailureLog[i];
            colored(ui, Theme::Fail, "t = {:.2f}s  {} / {}", record.Time, record.Category, record.Name);
            coloredRaw(ui, Theme::Dim, record.Detail.View(), Onyx::TextMode_Wrapped);
        }
        ui->EndScroll();
    }

    // -- legend ----------------------------------------------------------------------------------------------------
    if (ui->PushTree("Legend and controls"))
    {
        const auto legend = [&](const char *text, const Onyx::Color &color, const char *meaning) {
            ui->PushPanel(Onyx::LayoutDirection_LeftToRight, Onyx::Overlay::CenterLeft);
            badge(ui, text, color);
            coloredRaw(ui, Theme::Text, meaning);
            ui->PopPanel();
        };
        legend("PASS", Theme::Pass, "automatic check holds this frame");
        legend("FAIL", Theme::Fail, "automatic check broken (also logged)");
        legend("TODO", Theme::Todo, "visual check waiting for your verdict");
        legend("GOOD", Theme::Pass, "visual check you marked as right");
        legend("BAD", Theme::Fail, "visual check you marked as broken");
        legend("INFO", Theme::Info, "measured value, nothing to judge");
        explain(ui, "WASD/QE move the camera of the hovered view (2D and 3D galleries). Space/LCtrl move up and down "
                    "in 3D, LShift toggles looking around. The mouse wheel zooms.");
        ui->PopTree();
    }

    ui->EndWindow();
}

void drawOverlay(App &app)
{
    drawControlCenter(app);
    panelDpi(app);
    panelViews(app);
    panelRendering(app);
    panelTextures(app);
    panelInput(app);
    panelResources(app);
    panelToolkit(app);
    panelPlatform(app);
    panelOverlay(app);

    if (app.ShowOverlayDemo)
        app.Ui->ShowDemo(&app.ShowOverlayDemo);
    if (app.ShowStyleEditor && app.Ui->BeginWindow("Style editor", &app.ShowStyleEditor))
    {
        app.Ui->ShowStyleEditor();
        app.Ui->EndWindow();
    }
}
} // namespace

// =====================================================================================================================
//  10. MAIN LOOP
// =====================================================================================================================

int main()
{
    Onyx::Initialize({.Platform = DemoPlatform});
    Onyx::Resources_CreateDefault();

    App app{};
    setup(app);

    while (Onyx::Running())
    {
        app.DeltaTime = Onyx::GetDeltaTime(app.Window).AsSeconds();
        app.Time += app.DeltaTime;
        app.FrameTimes[app.FrameTimeHead] = app.DeltaTime;
        app.FrameTimeHead = (app.FrameTimeHead + 1) % app.FrameTimes.GetSize();

        processEvents(app);
        controlCameras(app);

        // the scene is recorded before the checks, so checks that inspect recorded state (lights) see this frame
        drawScene(app);
        runChecks(app);
        drawOverlay(app);
        app.Ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
