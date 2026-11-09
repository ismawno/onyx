#include "engine.hpp"
#include "options.hpp"
#include "onyx/onyx.hpp"
#include "tkit/container/static_array.hpp"
#include <catch2/interfaces/catch_interfaces_capture.hpp>

namespace OnyxTest
{
// ---------------------------------------------------------------------------------------------------------------------
//  Finding windows
// ---------------------------------------------------------------------------------------------------------------------

// The overlay keeps its windows private and has no public lookup, and the engine needs one that never creates a window
// (BeginWindow would). Overlay::findWindow is exactly that. Names in an explicit instantiation are not access checked
// ([temp.spec.general]), which is how this reaches it without changing Onyx. A public Overlay::FindWindow would replace
// it (proposals/overlay-find-window.diff)
namespace
{
using FindWindowMember = Onyx::OverlayWindow *(Onyx::Overlay::*)(Onyx::LayoutId);
struct FindWindowTag
{
};
FindWindowMember findWindowMember(FindWindowTag);
template <FindWindowMember Member> struct FindWindowAccess
{
    friend FindWindowMember findWindowMember(FindWindowTag)
    {
        return Member;
    }
};
template struct FindWindowAccess<&Onyx::Overlay::findWindow>;
} // namespace

static Onyx::OverlayWindow *findWindow(Onyx::Overlay *overlay, const usz id)
{
    return (overlay->*findWindowMember(FindWindowTag{}))(Onyx::LayoutId{id});
}
// begun on this frame. capture() runs between the gui and Draw, while this still holds
static bool isSubmitted(const Onyx::OverlayWindow *win)
{
    return win && win->SubmissionOrder != TKIT_U32_MAX;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Paths
// ---------------------------------------------------------------------------------------------------------------------

// what Overlay::IdFromStack does: the parent is hashed as a LayoutId and the id as a plain integer (std::hash differs
// for them on some standard libraries)
static usz stackId(const usz parent, const usz id)
{
    return TKit::Hash(Onyx::LayoutId{parent}, id);
}
static usz labelId(const TKit::DynamicString &label)
{
    return Onyx::OverlayLabel(TKit::StringView{label.GetData(), label.GetSize()}).Id.Id;
}
static usz nameId(const char *name)
{
    return Onyx::LayoutId{name}.Id;
}

// "a/b\/c" -> {"a", "b/c"}
static TKit::DynamicArray<TKit::DynamicString> splitPath(const TKit::DynamicString &path)
{
    TKit::DynamicArray<TKit::DynamicString> segments{};
    TKit::DynamicString current{};
    for (usz i = 0; i < path.GetSize(); ++i)
    {
        const char c = path[i];
        if (c == '\\' && i + 1 < path.GetSize() && path[i + 1] == '/')
        {
            current += '/';
            ++i;
        }
        else if (c == '/')
        {
            if (!current.IsEmpty())
                segments.Append(current);
            current.Clear();
        }
        else
            current += c;
    }
    if (!current.IsEmpty())
        segments.Append(current);
    return segments;
}

static ItemInfo toItem(Onyx::OverlayWindow *win, const Onyx::LayoutElementQueryInfo *info)
{
    ItemInfo item{};
    if (!info)
        return item;
    // the visible part: a clipped widget is hit where it shows
    const f32v2 wmin = Math::Max(info->Position, info->ClipMin);
    const f32v2 wmax = Math::Min(info->Position + info->Size, info->ClipMax);
    const f32v2 a = win->ToLocalScreen(wmin);
    const f32v2 b = win->ToLocalScreen(wmax);
    const f32v2 fa = win->ToLocalScreen(info->Position);
    const f32v2 fb = win->ToLocalScreen(info->Position + info->Size);
    item.Found = true;
    item.Id = info->Id;
    item.Window = win;
    item.Min = Math::Min(a, b);
    item.Max = Math::Max(a, b);
    item.FullMin = Math::Min(fa, fb);
    item.FullMax = Math::Max(fa, fb);
    return item;
}

TKit::DynamicString UiEngine::absolute(const char *path) const
{
    const TKit::DynamicString p{path};
    if (p.StartsWith("//"))
        return p.SubString(2);
    if (m_Ref.IsEmpty())
        return p;
    return TKit::DynamicString::Format("{}/{}", m_Ref, p);
}

ItemInfo UiEngine::resolveItem(const TKit::DynamicString &path) const
{
    const TKit::DynamicArray<TKit::DynamicString> segments = splitPath(path);
    if (segments.IsEmpty())
        return {};

    // a root window begins with an empty id stack: its label's id is both its window id and the top of the stack
    usz chain = labelId(segments[0]);
    Onyx::OverlayWindow *root = findWindow(m_Overlay, chain);
    if (!isSubmitted(root))
        return {};
    if (segments.GetSize() == 1)
    {
        ItemInfo item{};
        item.Found = true;
        item.Id = root->Id;
        item.Window = root;
        item.Min = item.FullMin = root->ScreenPos;
        item.Max = item.FullMax = root->ScreenPos + root->Size;
        return item;
    }

    // windows met along the way (child windows, popups): the item lives in the layout of the innermost one
    TKit::StaticArray<Onyx::OverlayWindow *, 16> windows{};
    windows.Append(root);
    usz parent = chain;
    for (usz i = 1; i < segments.GetSize(); ++i)
    {
        parent = chain;
        if (segments[i] == "##MenuBar")
        {
            chain = stackId(chain, windows.GetBack()->MenuBarId.Id);
            continue;
        }
        const usz id = labelId(segments[i]);
        chain = stackId(chain, id);
        if (i + 1 == segments.GetSize() || windows.GetSize() == windows.GetCapacity())
            continue;
        // child windows and popups are found by their label's id, or by the stacked one with MergeIdWithStack
        Onyx::OverlayWindow *win = findWindow(m_Overlay, id);
        if (!isSubmitted(win))
            win = findWindow(m_Overlay, chain);
        if (isSubmitted(win) && win != windows.GetBack())
            windows.Append(win);
    }

    // what to hit for the widget the path names, most specific first: the header of a tab (drawn by its tab bar, as
    // endTabBar builds it: a selectable given IdFromStack(tab id), under the id of the bar's content area, which PushId
    // stacks once more), the box of a text input or a dropdown, and the widget itself for everything else
    const usz contentArea = stackId(parent, stackId(parent, nameId("__onyx_id_Content_area")));
    const usz candidates[] = {
        stackId(contentArea, stackId(contentArea, chain)),
        stackId(chain, nameId("__onyx_id_Input_box")),
        stackId(chain, nameId("__onyx_id__Drop_down_box")),
        chain,
    };
    for (u32 i = windows.GetSize() - 1; i < windows.GetSize(); --i)
    {
        Onyx::OverlayWindow *win = windows[i];
        const Onyx::Layout *layout = win->GetActiveLayout();
        for (const usz id : candidates)
            if (const Onyx::LayoutElementQueryInfo *info = layout->QueryElement(Onyx::LayoutId{id}))
                return toItem(win, info);
    }
    return {};
}

WindowInfo UiEngine::resolveWindow(const TKit::DynamicString &path) const
{
    const TKit::DynamicArray<TKit::DynamicString> segments = splitPath(path);
    if (segments.IsEmpty())
        return {};
    usz chain = labelId(segments[0]);
    Onyx::OverlayWindow *win = findWindow(m_Overlay, chain);
    for (usz i = 1; i < segments.GetSize() && win; ++i)
    {
        const usz id = labelId(segments[i]);
        chain = stackId(chain, id);
        Onyx::OverlayWindow *child = findWindow(m_Overlay, id);
        win = child ? child : findWindow(m_Overlay, chain);
    }
    if (!isSubmitted(win))
        return {};

    WindowInfo info{};
    info.Found = true;
    info.Window = win;
    // a window that does not own its native (the usual case: no promotions) keeps its position in the native's local
    // logical pixels, the space the engine works in
    info.Min = win->ScreenPos;
    info.Max = win->ScreenPos + win->Size;
    if (win->HeaderId != Onyx::NullLayoutId)
        info.Header = toItem(win, win->GetActiveLayout()->QueryElement(win->HeaderId));
    return info;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Engine
// ---------------------------------------------------------------------------------------------------------------------

static Speed parseSpeed(const TKit::DynamicString &speed)
{
    if (speed == "normal")
        return Speed_Normal;
    if (speed == "cinematic")
        return Speed_Cinematic;
    return Speed_Fast;
}

UiEngine::UiEngine(const u32v2 &size, const Onyx::OverlayFlags flags) : m_Speed(parseSpeed(GetOptions().Speed))
{
    // named after the test, for whoever watches a slow run
    const TKit::DynamicString title =
        TKit::DynamicString::Format("onyx-tests: {}", Catch::getResultCapture().getCurrentTestName());
    m_Window = OpenTestWindow({.Size = size, .Flags = Onyx::WindowFlag_VirtualInput, .Title = title.CString()});
    Onyx::OverlaySpecs specs{};
    specs.Flags = flags;
    m_Overlay = m_Window->CreateOverlay(specs);

    // the injected mouse moves no real cursor: draw one, in a view of its own above the overlay's
    if (m_Speed != Speed_Fast)
    {
        m_CursorCamera.Mode = Onyx::CameraMode_Viewport;
        m_CursorView = m_Window->CreateRenderView<D2>(&m_CursorCamera);
        m_CursorView->ClearColor = Onyx::Color_Transparent;
        m_CursorContext = Onyx::CreateRenderContext<D2>();
        m_CursorContext->Flush();
        m_CursorContext->AddTarget(m_CursorView);
    }

    // render once before the first gui frame: views only get their viewport on their first render, and windows
    // declared before it are clamped against a placeholder (overlay.windows_declared_before_the_first_render_keep_
    // their_place covers that bug)
    Frame(1);
    // a press counts as part of a double click only if it follows another within the click window. the first one
    // must never look like a second one
    m_SincePress.Restart();
    m_SinceRelease.Restart();
    m_FrameClock.Restart();
}
UiEngine::~UiEngine()
{
    if (m_CursorContext)
    {
        Onyx::DestroyRenderContext(m_CursorContext);
        m_Window->DestroyRenderView(m_CursorView);
    }
    Onyx::CloseWindow(m_Window);
}

void UiEngine::noteText(const TKit::DynamicString &text)
{
    TKIT_LOG_INFO("[ONYX-TESTS][ENGINE] {}", text);
    UNSCOPED_INFO("Engine: " << text.CString());
}

void UiEngine::frame()
{
    Onyx::Running();
    if (Gui)
        Gui(m_Overlay);
    capture();
    m_Overlay->Draw();
    drawCursor();
    Onyx::Transfer();
    Onyx::Render();
    pace();
    ++m_Frame;
}
void UiEngine::Yield(const u32 frames)
{
    for (u32 i = 0; i < frames; ++i)
        frame();
}

void UiEngine::capture()
{
    if (!m_WatchItem.IsEmpty())
        m_ItemResult = resolveItem(m_WatchItem);
    if (!m_WatchWindow.IsEmpty())
        m_WindowResult = resolveWindow(m_WatchWindow);
}

void UiEngine::drawCursor()
{
    if (!m_CursorContext)
        return;
    m_CursorContext->Flush();
    if (m_Mouse[0] < 0.f)
        return;
    // a dot with a dark rim, orange while a button is held. transforms are extrinsic: scale, then move
    const f32v2 at = m_CursorView->ScreenToWorld(m_Mouse);
    const auto dot = [&](const f32 diameter, const Onyx::Color &color) {
        m_CursorContext->Push();
        m_CursorContext->Scale(diameter);
        m_CursorContext->Translate(at);
        m_CursorContext->FillColor(color);
        m_CursorContext->Circle();
        m_CursorContext->Pop();
    };
    dot(18.f, Onyx::Color_Black);
    dot(13.f, m_Pressed ? Onyx::Color_Orange : Onyx::Color_White);
}

// about 60 frames a second at the speeds meant to be watched. fast runs frames as fast as they go
void UiEngine::pace()
{
    if (m_Speed == Speed_Fast)
        return;
    constexpr f64 target = 1000.0 / 60.0;
    const f64 elapsed = m_FrameClock.GetElapsed().AsMilliseconds<f64>();
    if (elapsed < target)
        TKit::Timespan::Sleep(TKit::Timespan::FromMilliseconds(target - elapsed));
    m_FrameClock.Restart();
}
// a beat between actions, so a cinematic run can be followed
void UiEngine::pause(const f64 milliseconds)
{
    if (m_Speed != Speed_Cinematic)
        return;
    const u32 frames = u32(milliseconds / (1000.0 / 60.0));
    Yield(frames == 0 ? 1 : frames);
}

bool UiEngine::ResizeNative(const u32v2 &size, const u32 maxFrames)
{
    note("resize the native window to {}", size);
    m_Window->SetSize(size);
    for (u32 i = 0; i < maxFrames; ++i)
    {
        Yield();
        if (m_Window->GetSize() == size)
        {
            // the overlay sees the resize event on the next draw, and lays out for it on the one after
            Yield(3);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Items
// ---------------------------------------------------------------------------------------------------------------------

void UiEngine::SetRef(const char *path)
{
    m_Ref = path;
}

// a window begun for the first time is laid out on its first draw: an item is only on screen a frame after it is first
// declared. a few frames cover that, and a gui that opens something in reaction to the last action
ItemInfo UiEngine::GetItem(const char *path)
{
    m_WatchItem = absolute(path);
    for (u32 i = 0; i < 3; ++i)
    {
        Yield();
        if (m_ItemResult.Found)
            break;
    }
    m_WatchItem.Clear();
    return m_ItemResult;
}
WindowInfo UiEngine::GetWindowInfo(const char *path)
{
    m_WatchWindow = absolute(path);
    for (u32 i = 0; i < 3; ++i)
    {
        Yield();
        if (m_WindowResult.Found)
            break;
    }
    m_WatchWindow.Clear();
    return m_WindowResult;
}

ItemInfo UiEngine::waitItem(const char *path, const char *action)
{
    const TKit::DynamicString full = absolute(path);
    m_WatchItem = full;
    ItemInfo last{};
    for (u32 i = 0; i < 120; ++i)
    {
        Yield();
        const ItemInfo &now = m_ItemResult;
        if (now.Found && last.Found && Math::Approximately(now.Min, last.Min, 0.5f) &&
            Math::Approximately(now.Max, last.Max, 0.5f))
            break;
        last = now;
    }
    m_WatchItem.Clear();
    if (!m_ItemResult.Found)
        FAIL(TKit::Format("Engine: {} '{}': no such item on screen. Its window was not submitted, or nothing in it has "
                          "that id",
                          action, full));
    return m_ItemResult;
}
WindowInfo UiEngine::waitWindow(const char *path, const char *action)
{
    const TKit::DynamicString full = absolute(path);
    m_WatchWindow = full;
    WindowInfo last{};
    for (u32 i = 0; i < 120; ++i)
    {
        Yield();
        const WindowInfo &now = m_WindowResult;
        if (now.Found && last.Found && Math::Approximately(now.Min, last.Min, 0.5f) &&
            Math::Approximately(now.Max, last.Max, 0.5f))
            break;
        last = now;
    }
    m_WatchWindow.Clear();
    if (!m_WindowResult.Found)
        FAIL(TKit::Format("Engine: {} '{}': no such window was submitted", action, full));
    return m_WindowResult;
}

// ---------------------------------------------------------------------------------------------------------------------
//  Mouse
// ---------------------------------------------------------------------------------------------------------------------

void UiEngine::MouseMove(const f32v2 &position, u32 steps)
{
    const f32v2 from = m_Mouse[0] < 0.f ? position : m_Mouse;
    if (steps == 0)
    {
        // pixels a frame: a quick hand, a relaxed one, and one slow enough to follow
        const f32 speed = m_Speed == Speed_Fast ? TKIT_F32_MAX : (m_Speed == Speed_Normal ? 20.f : 8.f);
        const f32 distance = Math::Norm(position - from);
        const f32 frames = distance / speed;
        steps = frames < 3.f ? 3 : u32(frames);
    }
    for (u32 i = 1; i <= steps; ++i)
    {
        m_Mouse = from + (position - from) * (f32(i) / f32(steps));
        m_Window->InjectMouseMove(m_Mouse);
        Yield();
    }
}

void UiEngine::waitClickWindow()
{
    const f64 window = f64(m_Overlay->GetStyle(Onyx::OverlayStyle_ClickMilliseconds)) + 30.0;
    const f64 sincePress = m_SincePress.GetElapsed().AsMilliseconds<f64>();
    const f64 sinceRelease = m_SinceRelease.GetElapsed().AsMilliseconds<f64>();
    const f64 elapsed = sincePress < sinceRelease ? sincePress : sinceRelease;
    if (elapsed < window)
        TKit::Timespan::Sleep(TKit::Timespan::FromMilliseconds(window - elapsed));
}
void UiEngine::MouseDown(const Onyx::Mouse button)
{
    waitClickWindow();
    m_Window->InjectMouseButton(button, true);
    m_Pressed = true;
    m_SincePress.Restart();
    Yield();
}
void UiEngine::MouseUp(const Onyx::Mouse button)
{
    m_Window->InjectMouseButton(button, false);
    m_Pressed = false;
    m_SinceRelease.Restart();
    Yield();
}
void UiEngine::MouseClick(const Onyx::Mouse button)
{
    MouseDown(button);
    MouseUp(button);
    // the overlay sees the release when it draws, the gui reacts on the next frame, and anything that appears because
    // of it is on screen (and can be looked up) one frame after that
    Yield(3);
    pause(300.0);
}
void UiEngine::MouseClickAt(const f32v2 &position, const Onyx::Mouse button)
{
    note("click at {}", position);
    MouseMove(position);
    MouseClick(button);
}
void UiEngine::MouseDragTo(const f32v2 &position, const u32 steps)
{
    note("drag from {} to {}", m_Mouse, position);
    MouseDown();
    MouseMove(position, steps == 0 && m_Speed == Speed_Fast ? 8 : steps);
    MouseUp();
    Yield(2);
    pause(300.0);
}
void UiEngine::MouseWheel(const f32v2 &offset)
{
    note("scroll by {}", offset);
    m_Window->InjectScroll(offset);
    Yield(2);
}

void UiEngine::ItemHover(const char *path)
{
    const ItemInfo item = waitItem(path, "hover");
    note("hover '{}' at {}", absolute(path), item.Center());
    MouseMove(item.Center());
}
void UiEngine::ItemClick(const char *path, const Onyx::Mouse button)
{
    const ItemInfo item = waitItem(path, "click");
    note("{}click '{}' at {}", button == Onyx::Mouse_ButtonLeft ? "" : "right ", absolute(path), item.Center());
    MouseMove(item.Center());
    MouseClick(button);
}
void UiEngine::ItemCtrlClick(const char *path, const u32 heldFrames)
{
    const ItemInfo item = waitItem(path, "ctrl+click");
    note("ctrl+click '{}' at {}{}", absolute(path), item.Center(),
         heldFrames == 0 ? TKit::DynamicString{} : TKit::DynamicString::Format(", held {} frames", heldFrames));
    MouseMove(item.Center());
    KeyDown(Onyx::Key_LeftControl);
    MouseDown();
    Yield(heldFrames);
    MouseUp();
    KeyUp(Onyx::Key_LeftControl);
    Yield(3);
    pause(300.0);
}
void UiEngine::ItemDoubleClick(const char *path)
{
    const ItemInfo item = waitItem(path, "double click");
    note("double click '{}' at {}", absolute(path), item.Center());
    MouseMove(item.Center());
    MouseDown();
    MouseUp();
    // the second press must land inside the click window: no waiting here
    m_Window->InjectMouseButton(Onyx::Mouse_ButtonLeft, true);
    m_Pressed = true;
    Yield();
    MouseUp();
    Yield(2);
    pause(300.0);
}
void UiEngine::ItemDrag(const char *path, const f32v2 &delta)
{
    const ItemInfo item = waitItem(path, "drag");
    note("drag '{}' by {}", absolute(path), delta);
    MouseMove(item.Center());
    MouseDragTo(item.Center() + delta);
}
void UiEngine::ItemDragAndDrop(const char *source, const char *target)
{
    const ItemInfo from = waitItem(source, "drag");
    note("drag '{}' onto '{}'", absolute(source), absolute(target));
    MouseMove(from.Center());
    MouseDown();
    // a few pixels first, so the overlay sees a drag start on the source
    MouseMove(from.Center() + f32v2{6.f, 0.f}, 3);
    const ItemInfo to = waitItem(target, "drop onto");
    MouseMove(to.Center(), m_Speed == Speed_Fast ? 12 : 0);
    MouseUp();
    Yield(2);
    pause(300.0);
}
void UiEngine::ItemInput(const char *path, const char *text)
{
    const ItemInfo item = waitItem(path, "input into");
    note("input '{}' into '{}'", text, absolute(path));
    // sliders and drags need a ctrl+click to turn into a text box. text inputs take it as a plain click
    MouseMove(item.Center());
    KeyDown(Onyx::Key_LeftControl);
    MouseDown();
    MouseUp();
    KeyUp(Onyx::Key_LeftControl);
    Yield(2);
    // replace what it holds: select it all and type over it
    KeyPress(Onyx::Key_Home);
    KeyDown(Onyx::Key_LeftShift);
    KeyPress(Onyx::Key_End);
    KeyUp(Onyx::Key_LeftShift);
    KeyPress(Onyx::Key_Backspace);
    KeyChars(text);
    KeyPress(Onyx::Key_Enter);
    Yield(2);
    pause(300.0);
}

void UiEngine::MenuClick(const char *path)
{
    const TKit::DynamicArray<TKit::DynamicString> segments = splitPath(TKit::DynamicString{path});
    TKIT_ASSERT(!segments.IsEmpty(), "[ONYX-TESTS][ENGINE] MenuClick needs a path, as in 'File/Open'");
    note("menu '{}'", path);
    TKit::DynamicString at{"##MenuBar"};
    for (usz i = 0; i < segments.GetSize(); ++i)
    {
        at = TKit::DynamicString::Format("{}/{}", at, segments[i]);
        const ItemInfo item = waitItem(at.CString(), "open the menu at");
        if (i == 0)
        {
            // a header of the bar: a click opens its menu
            MouseMove(item.Center());
            MouseClick();
            continue;
        }
        // into the menu: straight down from where the mouse is, a few pixels a frame, then across onto the item. an
        // open menu closes when the cursor crosses the gap below its header too quickly
        // (overlay.menus_survive_a_quick_move_onto_their_items)
        const f32v2 below{m_Mouse[0], item.Min[1] + 2.f};
        const f32 distance = Math::Norm(below - m_Mouse);
        MouseMove(below, distance < 36.f ? 12 : u32(distance / 3.f));
        MouseMove(item.Center(), 4);
        if (i + 1 == segments.GetSize())
            MouseClick();
        else
            Yield(3);
    }
}

void UiEngine::WindowMove(const char *path, const f32v2 &position)
{
    const WindowInfo win = waitWindow(path, "move");
    if (!win.Header.Found)
        FAIL(TKit::Format("Engine: move '{}': the window has no header to drag it by", absolute(path)));
    note("move window '{}' from {} to {}", absolute(path), win.Min, position);
    const f32v2 grab = win.Header.Center();
    MouseMove(grab);
    MouseDragTo(grab + (position - win.Min), m_Speed == Speed_Fast ? 10 : 0);
}
void UiEngine::WindowResize(const char *path, const f32v2 &size)
{
    const WindowInfo win = waitWindow(path, "resize");
    note("resize window '{}' from {} to {}", absolute(path), win.Size(), size);
    const f32v2 corner = win.Max - f32v2{2.f};
    MouseMove(corner);
    MouseDragTo(corner + (size - win.Size()), m_Speed == Speed_Fast ? 10 : 0);
}

// ---------------------------------------------------------------------------------------------------------------------
//  Keyboard
// ---------------------------------------------------------------------------------------------------------------------

void UiEngine::KeyDown(const Onyx::Key key)
{
    m_Window->InjectKey(key, true);
    Yield();
}
void UiEngine::KeyUp(const Onyx::Key key)
{
    m_Window->InjectKey(key, false);
    Yield();
}
void UiEngine::KeyPress(const Onyx::Key key)
{
    KeyDown(key);
    KeyUp(key);
    Yield();
}
void UiEngine::KeyChars(const char *text)
{
    note("type '{}'", text);
    for (const char *c = text; *c; ++c)
    {
        m_Window->InjectChar(Onyx::CodePoint(u8(*c)));
        Yield(m_Speed == Speed_Cinematic ? 6 : 1);
    }
    Yield();
}
} // namespace OnyxTest
