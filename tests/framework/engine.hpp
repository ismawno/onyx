#pragma once

// The overlay test engine: a person at the mouse and keyboard of an overlay, in the spirit of Dear ImGui's test engine.
//
// A test hands the engine its gui (Gui, declared again every frame: immediate mode) and then plays the user from the
// test body, what Dear ImGui calls the test function. Actions name items by path, the way a person names what they see:
//
//   ui.SetRef("Settings");
//   ui.ItemClick("Apply");                 // a button in the window "Settings"
//   ui.ItemInput("Audio/Volume", "0.5");   // a slider under PushId("Audio") (or a tree node "Audio")
//   ui.MenuClick("File/Open");             // its menu bar: open "File", then click "Open"
//   ui.ItemClick("//Popup/Close");         // "//" starts from the root: a popup is a window of its own
//
// Paths are the ids the overlay builds, so they follow the same rules as labels: "Same##a" is its own item, "###id"
// names one by id alone, and "a\/b" is a label with a slash. Child windows, popups, menus, dropdowns, tab bars and
// trees all push ids, so their items are reached through them: "Test/Tabs/Two" is the header of the tab "Two" in the
// tab bar "Tabs". "##MenuBar" stands for a window's menu bar. Two more stand for what docking adds to a window and no
// label names: "Window/##DockTab" is the tab of a docked window in the tab bar of its dock node, and
// "Window/##DockDivider" the divider between its node and the one it is split with.
//
// Every action runs the frames it needs, injecting what a real mouse and keyboard send through a window with virtual
// input (so the desktop's own input never reaches it), and the overlay sees hovers, presses, holds and releases as it
// would on a desktop. The test owns the loop, so there are no coroutines: an action returns once its frames ran.
//
// An item is looked up the way it is on screen: in the layout the overlay drew on the last frame, of a window the gui
// submitted on this one. Before acting on it the engine waits until it holds still (the same place two frames in a
// row), so a window manager resizing the window, or a layout that settles over a few frames, does not make it miss.
//
// Every action is logged (Toolkit logging, [ONYX-TESTS][ENGINE]) and attached to the next check: a failed check lists
// what the engine did since the one before it.
//
// --speed fast (the default) moves the mouse in a few frames, as fast as frames go. normal and cinematic move it like a
// person would, pace frames to about 60 a second and draw the cursor, so a run can be watched.

#include "harness.hpp"
#include "options.hpp"
#include "onyx/overlay.hpp"
#include "tkit/profiling/clock.hpp"

namespace OnyxTest
{
// an item as the overlay laid it out on the last frame, in logical pixels of the native window. Min/Max are its visible
// part (clipped widgets are hit where they show), FullMin/FullMax all of it, wherever it was scrolled to
struct ItemInfo
{
    bool Found = false;
    Onyx::LayoutId Id{};
    Onyx::OverlayWindow *Window = nullptr;
    f32v2 Min{0.f};
    f32v2 Max{0.f};
    f32v2 FullMin{0.f};
    f32v2 FullMax{0.f};

    f32v2 Center() const
    {
        return 0.5f * (Min + Max);
    }
    f32v2 Size() const
    {
        return Max - Min;
    }
};

struct WindowInfo
{
    bool Found = false;
    Onyx::OverlayWindow *Window = nullptr;
    f32v2 Min{0.f};
    f32v2 Max{0.f};
    // the header bar, to drag the window by. not found for windows without one
    ItemInfo Header{};
    // in a dock node, whether its tab is the selected one or not
    bool Docked = false;

    f32v2 Size() const
    {
        return Max - Min;
    }
};

class UiEngine
{
  public:
    explicit UiEngine(const u32v2 &size = {640, 480}, Onyx::OverlayFlags flags = 0);
    UiEngine(const UiEngine &) = delete;
    UiEngine &operator=(const UiEngine &) = delete;
    ~UiEngine();

    Function<void(Onyx::Overlay *)> Gui;

    Onyx::Window *GetWindow() const
    {
        return m_Window;
    }
    Onyx::Overlay *GetOverlay() const
    {
        return m_Overlay;
    }
    u64 GetFrame() const
    {
        return m_Frame;
    }
    Speed GetSpeed() const
    {
        return m_Speed;
    }
    const f32v2 &GetMousePosition() const
    {
        return m_Mouse;
    }

    // -----------------------------------------------------------------------------------------------------------------
    //  Frames
    // -----------------------------------------------------------------------------------------------------------------

    // runs frames: poll, gui, overlay draw, transfer, render
    void Yield(u32 frames = 1);
    // runs frames until the condition holds (checked after each one). false if it never did
    template <typename F> bool YieldUntil(F &&condition, const u32 maxFrames = 240)
    {
        for (u32 i = 0; i < maxFrames; ++i)
        {
            Yield();
            if (condition())
                return true;
        }
        return false;
    }
    // resizes the native window, as a compositor would (a tiling one does it right after mapping a window), and runs
    // frames until the new size is in. false if it never was
    bool ResizeNative(const u32v2 &size, u32 maxFrames = 240);

    // -----------------------------------------------------------------------------------------------------------------
    //  Items and windows
    // -----------------------------------------------------------------------------------------------------------------

    // the base of relative paths (those that do not start with "//"), usually a window. empty for none
    void SetRef(const char *path);
    // where the item is right now. Found is false if it is not on screen. runs a frame, or up to 3 for an item that
    // is not there yet (a window is laid out on the frame after it is first declared)
    ItemInfo GetItem(const char *path);
    bool ItemExists(const char *path)
    {
        return GetItem(path).Found;
    }
    // the window at the end of the path ("Test", or "Test/Child" for a child window). frames as GetItem
    WindowInfo GetWindowInfo(const char *path);

    // -----------------------------------------------------------------------------------------------------------------
    //  Mouse. Positions are logical pixels of the native window
    // -----------------------------------------------------------------------------------------------------------------

    // moves there gradually, as a hand would: at fast speed in a few frames, at the others at a person's pace. steps
    // forces the number of frames (a slow, careful move, or a single frame jump)
    void MouseMove(const f32v2 &position, u32 steps = 0);
    void MouseDown(Onyx::Mouse button = Onyx::Mouse_ButtonLeft);
    void MouseUp(Onyx::Mouse button = Onyx::Mouse_ButtonLeft);
    // a click where the mouse is
    void MouseClick(Onyx::Mouse button = Onyx::Mouse_ButtonLeft);
    void MouseClickAt(const f32v2 &position, Onyx::Mouse button = Onyx::Mouse_ButtonLeft);
    // press where the mouse is, move there and release
    void MouseDragTo(const f32v2 &position, u32 steps = 0);
    void MouseWheel(const f32v2 &offset);

    void ItemHover(const char *path);
    void ItemClick(const char *path, Onyx::Mouse button = Onyx::Mouse_ButtonLeft);
    // a click with ctrl held, which turns sliders and drags into text boxes. heldFrames keeps the button down for
    // longer
    void ItemCtrlClick(const char *path, u32 heldFrames = 0);
    void ItemDoubleClick(const char *path);
    // press on the item, move by delta and release
    void ItemDrag(const char *path, const f32v2 &delta);
    // press on the source, carry it onto the target and release there
    void ItemDragAndDrop(const char *source, const char *target);
    // types the text into a text input, a numeric input, a slider or a drag (ctrl+click turns those two into text
    // boxes), replacing what it held, and presses enter
    void ItemInput(const char *path, const char *text);

    // opens the menus along the path in the menu bar of the reference window, and clicks the last item: "File/Open".
    // the way from a header into its menu is taken slowly and straight down, as a careful hand does
    void MenuClick(const char *path);

    // drags the window by its header until its top left corner is there
    void WindowMove(const char *path, const f32v2 &position);
    // drags the window's bottom right corner until it has that size
    void WindowResize(const char *path, const f32v2 &size);
    // drags the window by its header and drops it with the mouse at that point: over another window's middle, edge or
    // header, that is how a person docks it
    void WindowDrop(const char *path, const f32v2 &mouse);

    // -----------------------------------------------------------------------------------------------------------------
    //  Keyboard
    // -----------------------------------------------------------------------------------------------------------------

    void KeyDown(Onyx::Key key);
    void KeyUp(Onyx::Key key);
    void KeyPress(Onyx::Key key);
    // types the text, one character a frame
    void KeyChars(const char *text);

  private:
    void frame();
    void capture();
    void drawCursor();
    void pace();
    void pause(f64 milliseconds);

    String absolute(const char *path) const;
    ItemInfo resolveItem(const String &path) const;
    WindowInfo resolveWindow(const String &path) const;
    // the item, once it holds still. fails the test if it never shows
    ItemInfo waitItem(const char *path, const char *action);
    WindowInfo waitWindow(const char *path, const char *action);

    // separate clicks must not land within the overlay's double click window, or they would count as one. the overlay
    // counts both presses and releases
    void waitClickWindow();
    // logs the action and attaches it to the next check
    template <typename... Args> void note(const fmt::format_string<Args...> format, Args &&...args)
    {
        noteText(String::Format(format, std::forward<Args>(args)...));
    }
    void noteText(const String &text);

    Onyx::Window *m_Window = nullptr;
    Onyx::Overlay *m_Overlay = nullptr;
    Speed m_Speed = Speed_Fast;
    String m_Ref{};

    // what capture() looks up on the next frame, and what it found
    String m_WatchItem{};
    String m_WatchWindow{};
    ItemInfo m_ItemResult{};
    WindowInfo m_WindowResult{};

    f32v2 m_Mouse{-1.f};
    bool m_Pressed = false;
    u64 m_Frame = 0;
    TKit::Clock m_SincePress{};
    TKit::Clock m_SinceRelease{};
    TKit::Clock m_FrameClock{};

    // the drawn cursor, at normal and cinematic speeds only
    Onyx::Camera<D2> m_CursorCamera{};
    Onyx::RenderView<D2> *m_CursorView = nullptr;
    Onyx::RenderContext<D2> *m_CursorContext = nullptr;
};
} // namespace OnyxTest
