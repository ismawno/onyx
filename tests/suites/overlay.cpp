// The overlay, used the way a person uses it. Every test declares a small gui and the overlay test engine (engine.hpp)
// plays the user: it finds widgets by path ("Test/Press me"), moves the mouse onto them, clicks, drags, scrolls and
// types, through a window with virtual input. Then the test checks what the gui reported back (a button press, a new
// value) and how windows reacted.

#include "engine.hpp"
#include <cmath>

using namespace OnyxTest;

// most tests use a single window of a known size, at the default position. only the size is set: setting position and
// size together loses the position (overlay.next_window_position_and_size_apply_together covers that bug)
static bool beginTestWindow(Onyx::Overlay *ov, const char *title = "Test", const f32v2 &size = {320.f, 260.f},
                            Onyx::OverlayWindowFlags flags = 0)
{
    ov->SetNextWindowSize(size, Onyx::OverlayCondition_OnBirth);
    return ov->BeginWindow(title, flags);
}

// ---------------------------------------------------------------------------------------------------------------------
//  Windows
// ---------------------------------------------------------------------------------------------------------------------

// Fails until fixed: every frame, processWindows calls ClampToNative on each window (without promotions), and it writes
// the clamped position back into ScreenPos. A native window that is small for a while (a tiling compositor's first
// tile, the 2x2 placeholder before the first render, a user shrinking it) pushes windows in for good: they stay against
// the edge when it grows again. On Hyprland this is what sends the overlay demo's window into a corner. Clamping the
// position a window is drawn at, while keeping the one it was given (or dragged to), fixes it
ONYX_TEST(overlay, windows_keep_their_place_through_a_native_resize,
          .Description = "A window near the bottom right corner of its native window goes back to the same place after "
                         "the native window shrinks and grows again, as a tiling compositor does to new windows")
{
    UiEngine ui{{640, 480}};
    ui.Gui = [&](Onyx::Overlay *ov) {
        // position only: setting position and size together loses the position
        // (overlay.next_window_position_and_size_apply_together)
        ov->SetNextWindowPosition({400.f, 300.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("Corner"))
        {
            ov->Text("Bottom right");
            ov->EndWindow();
        }
    };
    ui.Yield(2);
    const WindowInfo before = ui.GetWindowInfo("Corner");
    REQUIRE(before.Found);
    REQUIRE_THAT(before.Min, IsNear(f32v2{400.f, 300.f}, 2.f));

    REQUIRE(ui.ResizeNative({320, 240}));
    REQUIRE(ui.ResizeNative({640, 480}));
    const f32v2 after = ui.GetWindowInfo("Corner").Min;
    CHECK_MSG(Math::Norm(after - before.Min) < 2.f,
              "The window was at {} before the native window shrank, and is at {} once it grew back", before.Min,
              after);
}

ONYX_TEST(overlay, windows_appear_where_placed,
          .Description = "A window placed with SetNextWindowPosition shows there, and captures the mouse only while "
                         "the mouse is over it")
{
    UiEngine ui{};
    bool capturing = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        ov->SetNextWindowPosition({50.f, 60.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("Placed"))
        {
            ov->Text("Hello");
            ov->EndWindow();
        }
        capturing = ov->WantCaptureMouse();
    };
    ui.Yield(2);
    const WindowInfo win = ui.GetWindowInfo("Placed");
    REQUIRE(win.Found);
    CHECK_THAT(win.Min, IsNear(f32v2(50.f, 60.f), 2.f));

    ui.MouseMove(0.5f * (win.Min + win.Max));
    CHECK(capturing);
    ui.MouseMove(f32v2{500.f, 400.f});
    CHECK(!capturing);
}

ONYX_TEST(overlay, window_sizes_apply,
          .Description = "SetNextWindowSize gives a window its size when it is born, and leaves it alone afterwards")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        ov->SetNextWindowSize({300.f, 200.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("Sized"))
            ov->EndWindow();
    };
    ui.Yield(2);
    const WindowInfo win = ui.GetWindowInfo("Sized");
    REQUIRE(win.Found);
    CHECK_THAT(win.Size(), IsNear(f32v2(300.f, 200.f), 1.f));
}

// Fails until fixed: the next window data has one condition for both, and BeginWindow consumes it when it applies the
// size, before it looks at the position: the position is dropped for any condition but Always. Checking the condition
// once for both fixes it
ONYX_TEST(overlay, next_window_position_and_size_apply_together,
          .Description = "SetNextWindowPosition and SetNextWindowSize with the same condition both apply")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        ov->SetNextWindowPosition({50.f, 60.f}, Onyx::OverlayCondition_OnBirth);
        ov->SetNextWindowSize({300.f, 200.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("Placed and sized"))
            ov->EndWindow();
    };
    ui.Yield(2);
    const WindowInfo win = ui.GetWindowInfo("Placed and sized");
    REQUIRE(win.Found);
    CHECK_THAT(win.Size(), IsNear(f32v2(300.f, 200.f), 1.f));
    CHECK_THAT(win.Min, IsNear(f32v2(50.f, 60.f), 1.f));
}

// Fails until fixed: render views only get a viewport on their first render. Until then the overlay's native world is a
// 2x2 placeholder and ClampToNative pushes windows off to the top left (120, 120 becomes about -43, -47). Sizing views
// on creation, or not clamping against an unsized view, fixes it
ONYX_TEST(overlay, windows_declared_before_the_first_render_keep_their_place,
          .Description = "A window declared on the very first frame, before anything was rendered, is placed where "
                         "asked")
{
    // no engine and no render before the gui on purpose: the engine's own first render is what avoids the bug
    // everywhere else
    Onyx::Window *win = OpenTestWindow({.Size = {640, 480}, .Flags = Onyx::WindowFlag_VirtualInput});
    Onyx::Overlay *ov = win->CreateOverlay();
    f32v2 position{0.f};
    for (u32 i = 0; i < 3; ++i)
    {
        Onyx::Running();
        ov->SetNextWindowPosition({200.f, 150.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("Early"))
        {
            position = ov->GetActiveWindow()->ScreenPos;
            ov->EndWindow();
        }
        ov->Draw();
        Onyx::Transfer();
        Onyx::Render();
    }
    CHECK_THAT(position, IsNear(f32v2(200.f, 150.f), 1.f));
}

ONYX_TEST(overlay, its_view_covers_the_window,
          .Description = "The overlay draws through a view of its own that spans the whole window and follows resizes")
{
    UiEngine ui{{500, 400}};
    const Onyx::RenderView<D2> *view = ui.GetOverlay()->GetMainNativeWindow()->View;
    ui.Yield(2);
    CHECK_THAT(view->GetViewport().Position, IsNear(f32v2(0.f), 0.5f));
    CHECK_THAT(view->GetViewport().Size, IsNear(f32v2(500.f, 400.f), 0.5f));
    REQUIRE(ui.ResizeNative({600, 300}));
    CHECK_THAT(view->GetViewport().Size, IsNear(f32v2(600.f, 300.f), 0.5f));
}

ONYX_TEST(overlay, windows_move_by_their_header,
          .Description = "Dragging a window by its header moves it by the same amount")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov, "Movable", {240.f, 180.f}))
            return;
        ov->Text("Drag me by the header");
        ov->EndWindow();
    };
    const WindowInfo before = ui.GetWindowInfo("Movable");
    REQUIRE(before.Found);
    const f32v2 target = before.Min + f32v2{80.f, 50.f};
    ui.WindowMove("Movable", target);
    CHECK_THAT(ui.GetWindowInfo("Movable").Min, IsNear(target, 3.f));
}

ONYX_TEST(overlay, windows_resize_by_their_corner,
          .Description = "Dragging a window's bottom right corner grows it by the same amount")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov, "Resizable", {240.f, 180.f}))
            return;
        ov->EndWindow();
    };
    const WindowInfo before = ui.GetWindowInfo("Resizable");
    REQUIRE(before.Found);
    const f32v2 target = before.Size() + f32v2{60.f, 40.f};
    ui.WindowResize("Resizable", target);
    CHECK_THAT(ui.GetWindowInfo("Resizable").Size(), IsNear(target, 3.f));
}

ONYX_TEST(overlay, front_windows_take_the_clicks,
          .Description = "Where two windows overlap, the one on top gets the click")
{
    UiEngine ui{};
    u32 back = 0;
    u32 front = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (beginTestWindow(ov, "Back", {300.f, 200.f}))
        {
            if (ov->Button("Back button", Onyx::OverlayButtonFlag_SpanFullWidth))
                ++back;
            ov->EndWindow();
        }
        if (beginTestWindow(ov, "Front", {300.f, 200.f}))
        {
            if (ov->Button("Front button", Onyx::OverlayButtonFlag_SpanFullWidth))
                ++front;
            ov->EndWindow();
        }
    };
    // the later window is on top, and both buttons sit at the same place
    ui.ItemClick("Front/Front button");
    CHECK(front == 1u);
    CHECK(back == 0u);
}

ONYX_TEST(overlay, scrolling_moves_the_contents,
          .Description = "The mouse wheel scrolls a window that holds more than it can show")
{
    UiEngine ui{};
    TKit::DynamicArray<TKit::DynamicString> labels{};
    for (u32 i = 0; i < 30; ++i)
        labels.Append(TKit::DynamicString::Format("Item {}", i));
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov, "Scroll", {240.f, 160.f}))
            return;
        for (const TKit::DynamicString &label : labels)
            ov->Button(label.CString());
        ov->EndWindow();
    };
    ui.SetRef("Scroll");
    const ItemInfo before = ui.GetItem("Item 0");
    REQUIRE(before.Found);
    ui.ItemHover("Item 0");
    for (u32 i = 0; i < 5; ++i)
        ui.MouseWheel(f32v2{0.f, -1.f});
    ui.Yield(2);
    const ItemInfo after = ui.GetItem("Item 0");
    CHECK_MSG((!after.Found || after.FullMin[1] < before.FullMin[1] - 5.f),
              "The first item did not move up: from {} to {}", before.FullMin[1], after.FullMin[1]);
}

ONYX_TEST(overlay, dock_trees_lay_windows_out,
          .Description = "A submitted dock tree splits a dockspace between its windows: they share it without "
                         "overlapping and report being docked")
{
    UiEngine ui{{640, 480}, Onyx::OverlayFlag_Docking};
    bool submitted = false;
    bool leftDocked = false;
    bool rightDocked = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        const Onyx::LayoutId host = ov->FullScreenDockSpace();
        if (!submitted)
        {
            ov->SubmitDockTree(host, Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.5f, Onyx::DockTabBar("Left"),
                                                     Onyx::DockTabBar("Right")));
            submitted = true;
        }
        // docked windows are laid out by their dock node and do not update their own position: measure them through
        // a widget that spans their width
        if (ov->BeginWindow("Left"))
        {
            ov->Button("Left fill", Onyx::OverlayButtonFlag_SpanFullWidth);
            leftDocked = ov->IsCurrentWindowDocked();
            ov->EndWindow();
        }
        if (ov->BeginWindow("Right"))
        {
            ov->Button("Right fill", Onyx::OverlayButtonFlag_SpanFullWidth);
            rightDocked = ov->IsCurrentWindowDocked();
            ov->EndWindow();
        }
    };
    ui.Yield(5);
    const ItemInfo left = ui.GetItem("Left/Left fill");
    const ItemInfo right = ui.GetItem("Right/Right fill");
    REQUIRE((left.Found && right.Found));
    CHECK(leftDocked);
    CHECK(rightDocked);
    const bool apart = left.Max[0] <= right.Min[0] + 2.f || right.Max[0] <= left.Min[0] + 2.f ||
                       left.Max[1] <= right.Min[1] + 2.f || right.Max[1] <= left.Min[1] + 2.f;
    CHECK_MSG(apart, "Left {}..{} overlaps right {}..{}", left.Min, left.Max, right.Min, right.Max);
    // side by side or stacked, each gets a good share of the dockspace
    const f32 lw = left.Size()[0];
    const f32 rw = right.Size()[0];
    const bool sideBySide = left.Max[0] <= right.Min[0] + 2.f || right.Max[0] <= left.Min[0] + 2.f;
    CHECK_MSG((sideBySide ? (lw > 200.f && rw > 200.f) : (lw > 500.f && rw > 500.f)), "Widths {} and {}", lw, rw);
}

// Fails until fixed: PushStyleColor saves m_Style[color], which carries the current alpha (OverlayStyle_Alpha), and
// PopStyleColor writes that back as the color itself. Inside BeginDisabled the alpha is 0.6, so every push and pop
// there takes 40% off the color for good: a gui that does it every frame fades the color out in a few frames, and with
// OverlayColor_Text every widget title disappears (the control center's tests window did, while a run was going).
// Saving m_Style.Colors[color] fixes it
ONYX_TEST(overlay, style_colors_survive_a_push_inside_a_disabled_region,
          .Description = "Pushing and popping a style color between BeginDisabled and EndDisabled leaves the color as "
                         "it was")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->BeginDisabled();
        ov->PushStyleColor(Onyx::OverlayColor_Text, Onyx::Color_Red);
        ov->Text("Red, and dimmed as disabled");
        ov->PopStyleColor();
        ov->EndDisabled();
        ov->EndWindow();
    };
    const f32 before = ui.GetOverlay()->GetColor(Onyx::OverlayColor_Text).rgba[3];
    ui.Yield(10);
    const f32 after = ui.GetOverlay()->GetColor(Onyx::OverlayColor_Text).rgba[3];
    CHECK_MSG(Math::Approximately(after, before, 1e-3f), "The text color's alpha went from {} to {} in 10 frames",
              before, after);
}

// ---------------------------------------------------------------------------------------------------------------------
//  Buttons and toggles
// ---------------------------------------------------------------------------------------------------------------------

ONYX_TEST(overlay, buttons_fire_once_per_click,
          .Description = "A click on a button reports one press. Hovering does not, and neither does a press that "
                         "starts elsewhere")
{
    UiEngine ui{};
    u32 presses = 0;
    bool hovered = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Press me"))
            ++presses;
        hovered = ov->IsItemHovered();
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemHover("Press me");
    CHECK(hovered);
    CHECK(presses == 0u);

    ui.ItemClick("Press me");
    CHECK(presses == 1u);
    ui.Yield(3);
    CHECK(presses == 1u);

    // a press outside every window, released over the button
    ui.MouseMove(f32v2{600.f, 450.f});
    ui.MouseDragTo(ui.GetItem("Press me").Center());
    CHECK(presses == 1u);
}

// Fails until fixed: both events are consumed by the same Draw, so the widget never sees the press before the release
// and the click is lost. It shows on slow frames: under a software rasterizer (about 9 fps) the control center drops
// quick clicks
ONYX_TEST(overlay, clicks_shorter_than_a_frame_count,
          .Description = "A press and its release that arrive within the same frame (a quick tap, a slow frame) still "
                         "click the button under the mouse",
          .KnownIssue = "Requires a bit of a messy fix (queuing clicks) for an issue that is non-lethal and happens "
                        "only in 'extreme' scenarion")
{
    UiEngine ui{};
    u32 presses = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Tap me"))
            ++presses;
        ov->EndWindow();
    };
    ui.ItemHover("Test/Tap me");
    ui.GetWindow()->InjectMouseButton(Onyx::Mouse_ButtonLeft, true);
    ui.GetWindow()->InjectMouseButton(Onyx::Mouse_ButtonLeft, false);
    ui.Yield(4);
    CHECK(presses == 1u);
}

ONYX_TEST(overlay, clicks_land_while_the_native_window_resizes,
          .Description = "A button is clicked right after the native window changed size, as a tiling compositor does "
                         "to a window it just mapped: the engine waits for the layout to settle")
{
    UiEngine ui{};
    u32 presses = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Press me"))
            ++presses;
        ov->EndWindow();
    };
    ui.GetWindow()->SetSize({900, 700});
    ui.ItemClick("Test/Press me");
    CHECK(presses == 1u);
    ui.GetWindow()->SetSize({500, 400});
    ui.ItemClick("Test/Press me");
    CHECK(presses == 2u);
}

ONYX_TEST(overlay, checkboxes_toggle, .Description = "Each click on a checkbox flips its value")
{
    UiEngine ui{};
    bool value = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->CheckBox("Enabled", &value);
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Enabled");
    CHECK(value);
    ui.ItemClick("Enabled");
    CHECK(!value);
}

ONYX_TEST(overlay, radio_buttons_select_one, .Description = "Clicking a radio button selects its value")
{
    UiEngine ui{};
    u32 choice = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->RadioButton("First", &choice, 0u);
        ov->RadioButton("Second", &choice, 1u);
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Second");
    CHECK(choice == 1u);
    ui.ItemClick("First");
    CHECK(choice == 0u);
}

ONYX_TEST(overlay, selectables_toggle, .Description = "A selectable bound to a bool flips it on click")
{
    UiEngine ui{};
    bool selected = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->Selectable("Pick me", &selected);
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Pick me");
    CHECK(selected);
    ui.ItemClick("Pick me");
    CHECK(!selected);
}

ONYX_TEST(overlay, disabled_widgets_ignore_input,
          .Description = "Between BeginDisabled and EndDisabled clicks do nothing; after EndDisabled they work")
{
    UiEngine ui{};
    u32 disabledPresses = 0;
    u32 enabledPresses = 0;
    bool box = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->BeginDisabled();
        if (ov->Button("Disabled"))
            ++disabledPresses;
        ov->CheckBox("Disabled box", &box);
        ov->EndDisabled();
        if (ov->Button("Enabled"))
            ++enabledPresses;
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Disabled");
    ui.ItemClick("Disabled box");
    ui.ItemClick("Enabled");
    CHECK(disabledPresses == 0u);
    CHECK(!box);
    CHECK(enabledPresses == 1u);
}

ONYX_TEST(overlay, equal_labels_have_distinct_ids,
          .Description = "Two buttons with the same visible label but different ids (##) react separately")
{
    UiEngine ui{};
    u32 a = 0;
    u32 b = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Same##a"))
            ++a;
        if (ov->Button("Same##b"))
            ++b;
        ov->EndWindow();
    };
    ui.SetRef("Test");
    const ItemInfo first = ui.GetItem("Same##a");
    const ItemInfo second = ui.GetItem("Same##b");
    REQUIRE((first.Found && second.Found));
    CHECK(!Math::Approximately(first.Center(), second.Center(), 1.f));
    ui.ItemClick("Same##b");
    CHECK(a == 0u);
    CHECK(b == 1u);
}

ONYX_TEST(overlay, scopes_tell_equal_labels_apart,
          .Description = "Buttons with the same label under different id scopes (PushId, a tree node) are reached by "
                         "their paths and react separately")
{
    UiEngine ui{};
    u32 plain = 0;
    u32 grouped = 0;
    u32 nested = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Go"))
            ++plain;
        ov->PushId("Group");
        if (ov->Button("Go"))
            ++grouped;
        ov->PopId();
        if (ov->PushTree("Node", Onyx::OverlayTreeFlag_StartOpen))
        {
            if (ov->Button("Go"))
                ++nested;
            ov->PopTree();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Group/Go");
    ui.ItemClick("Node/Go");
    ui.ItemClick("Node/Go");
    ui.ItemClick("Go");
    CHECK(grouped == 1u);
    CHECK(nested == 2u);
    CHECK(plain == 1u);
}

ONYX_TEST(overlay, activation_keys_press_buttons,
          .Description = "A button given an activation key fires when that key is pressed")
{
    UiEngine ui{};
    u32 presses = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->SetNextActivationKey(Onyx::Key_B);
        if (ov->Button("Press B"))
            ++presses;
        ov->EndWindow();
    };
    // the key works while the window can take input: hover it
    ui.ItemHover("Test/Press B");
    ui.KeyPress(Onyx::Key_B);
    CHECK_MSG(presses >= 1u, "Pressing B did not press the button");
}

// ---------------------------------------------------------------------------------------------------------------------
//  Sliders, drags and text
// ---------------------------------------------------------------------------------------------------------------------

// a slider with its value, in a window of known size
static void sliderGui(UiEngine &ui, f32 &value)
{
    ui.Gui = [&value](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->HorizontalSlider("Slider", &value, 0.f, 1.f);
        ov->EndWindow();
    };
    ui.SetRef("Test");
}

// Fails until fixed: mustConvertToInputBox gives the box the active id, but on that frame the box has no layout yet,
// so queryAndSetInteraction leaves early and nothing marks the id to persist: processWindows clears it. The box then
// lives one more frame while hovered and turns back into a slider. A click that lasts more than a frame keeps it, as
// the press makes the box active again. Setting StateFlag_ActiveIdMustPersist where the conversion is triggered fixes
// it
ONYX_TEST(overlay, ctrl_click_turns_a_slider_into_a_text_box,
          .Description =
              "A quick ctrl+click on a slider opens a text box with the value's text, which stays open to be "
              "edited: End, a 5 and Enter turn 0.250 into 0.2505")
{
    UiEngine ui{};
    f32 value = 0.25f;
    sliderGui(ui, value);
    ui.ItemCtrlClick("Slider");
    // a click with ctrl held edits the value as text instead of dragging it
    REQUIRE_MSG(value == 0.25f, "Ctrl+click moved the slider to {}", value);

    ui.KeyPress(Onyx::Key_End);
    ui.KeyChars("5");
    ui.KeyPress(Onyx::Key_Enter);
    ui.Yield(2);
    CHECK_MSG(std::fabs(value - 0.2505f) < 1e-5f, "Editing the text gave {}: the box closed before it was typed into",
              value);
}

// Fails until fixed: on the frame a slider turns into a text box, the box has no layout yet and inputTextBox works
// with a box 0 pixels wide. scrollRightIfNeeded then scrolls the text by its whole width to show the cursor, and on
// later frames, with the real width, nothing scrolls it back: the box shows empty (the text is there, out of view) and
// clicks land a text width away from the character under the mouse. Leaving the scroll alone while the box has no
// layout (horScroll && ibox) fixes it
ONYX_TEST(overlay, slider_text_boxes_show_their_text,
          .Description =
              "The text box a ctrl+click opens on a slider scrolls to its text: a click at its left edge puts "
              "the cursor before the first character, so a 9 typed there turns 0.250 into 90.25")
{
    UiEngine ui{};
    f32 value = 0.25f;
    sliderGui(ui, value);
    // held, so the box stays open whatever overlay.ctrl_click_turns_a_slider_into_a_text_box finds
    ui.ItemCtrlClick("Slider", 6);
    REQUIRE(value == 0.25f);
    const ItemInfo box = ui.GetItem("Slider");
    REQUIRE(box.Found);
    ui.MouseClickAt(f32v2{box.Min[0] + 3.f, box.Center()[1]});
    ui.KeyChars("9");
    ui.KeyPress(Onyx::Key_Enter);
    ui.Yield(2);
    CHECK_MSG(std::fabs(value - 90.25f) < 1e-3f,
              "A 9 typed after a click at the left edge gave {}: 0.2509 means the click landed a text width to the "
              "right, the text scrolled out of view",
              value);
}

ONYX_TEST(overlay, sliders_follow_the_mouse,
          .Description = "Dragging a slider moves its value toward the mouse and clamps it to the range")
{
    UiEngine ui{};
    f32 value = 5.f;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->HorizontalSlider("Slider", &value, 0.f, 10.f);
        ov->EndWindow();
    };
    ui.SetRef("Test");
    const ItemInfo slider = ui.GetItem("Slider");
    REQUIRE(slider.Found);
    const f32 width = slider.Size()[0];

    ui.ItemDrag("Slider", f32v2{0.25f * width, 0.f});
    CHECK_MSG((value > 5.5f && value < 9.5f), "A quarter width to the right gave {}", value);
    ui.ItemDrag("Slider", f32v2{4.f * width, 0.f});
    CHECK_THAT(value, IsNear(10.f, 1e-4f));
    ui.ItemDrag("Slider", f32v2{-4.f * width, 0.f});
    CHECK_THAT(value, IsNear(0.f, 1e-4f));
}

ONYX_TEST(overlay, drags_change_values_by_mouse_motion,
          .Description = "A drag widget changes its value as the mouse moves while held, in the direction of the "
                         "motion")
{
    UiEngine ui{};
    f32 value = 0.f;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->HorizontalDrag("Drag", &value, 0.1f, -100.f, 100.f);
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemDrag("Drag", f32v2{60.f, 0.f});
    CHECK_MSG(value > 0.f, "Dragging right gave {}", value);
    const f32 after = value;
    ui.ItemDrag("Drag", f32v2{-120.f, 0.f});
    CHECK_MSG(value < after, "Dragging left went from {} to {}", after, value);
}

ONYX_TEST(overlay, text_inputs_take_typing,
          .Description = "A text input takes focus on click, accepts typed characters and deletes with backspace")
{
    UiEngine ui{};
    char buffer[64] = "";
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->InputText("Name", buffer, sizeof(buffer));
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Name");
    ui.KeyChars("hello");
    CHECK(Text(buffer) == "hello");
    ui.KeyPress(Onyx::Key_Backspace);
    CHECK(Text(buffer) == "hell");
}

ONYX_TEST(overlay, text_inputs_ignore_typing_without_focus,
          .Description = "Typing goes nowhere until a text input is clicked, and stops once the click lands elsewhere")
{
    UiEngine ui{};
    char buffer[64] = "";
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->InputText("Name", buffer, sizeof(buffer));
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemHover("Name");
    ui.KeyChars("lost");
    CHECK_MSG(buffer[0] == '\0', "Typed without focus, the buffer is '{}'", buffer);

    ui.ItemClick("Name");
    ui.KeyChars("ok");
    ui.MouseClickAt(f32v2{600.f, 450.f});
    ui.KeyChars("x");
    CHECK(Text(buffer) == "ok");
}

ONYX_TEST(overlay, read_only_inputs_keep_their_text,
          .Description = "A read only text input can be clicked but typing does not change it")
{
    UiEngine ui{};
    char buffer[32] = "fixed";
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->BeginReadOnly();
        ov->InputText("Read only", buffer, sizeof(buffer));
        ov->EndReadOnly();
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Read only");
    ui.KeyChars("abc");
    ui.KeyPress(Onyx::Key_Backspace);
    CHECK(Text(buffer) == "fixed");
}

ONYX_TEST(overlay, numeric_inputs_parse_what_is_typed,
          .Description = "A numeric input replaces its value with the number typed into it")
{
    UiEngine ui{};
    i32 value = 7;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->InputNumeric("Count", &value);
        ov->EndWindow();
    };
    ui.ItemInput("Test/Count", "42");
    CHECK(value == 42);
}

// ---------------------------------------------------------------------------------------------------------------------
//  Dropdowns, menus, popups, tooltips, tabs and trees
// ---------------------------------------------------------------------------------------------------------------------

ONYX_TEST(overlay, dropdowns_open_and_select,
          .Description = "A dropdown opens on click, shows its options and closes once one is picked")
{
    UiEngine ui{};
    u32 picked = 0;
    const char *options[] = {"Apple", "Banana", "Cherry"};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->BeginDropDown("Fruit", options[picked]))
        {
            for (u32 i = 0; i < 3; ++i)
                if (ov->Selectable(options[i], picked == i))
                {
                    picked = i;
                    // closing is up to the caller, as the one line DropDown does
                    ov->CloseCurrentPopup();
                }
            ov->EndDropDown();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    CHECK(!ui.ItemExists("Fruit/Banana"));
    ui.ItemClick("Fruit");
    REQUIRE_MSG(ui.ItemExists("Fruit/Banana"), "The options did not appear");
    ui.ItemClick("Fruit/Banana");
    CHECK(picked == 1u);
    CHECK_MSG(!ui.ItemExists("Fruit/Banana"), "The dropdown stayed open after picking");
}

// a window with a menu bar holding a "File" menu with an "Open" item
static void menuGui(Onyx::Overlay *ov, u32 &opened)
{
    if (!beginTestWindow(ov, "Test", {320.f, 260.f}, Onyx::OverlayWindowFlag_MenuBar))
        return;
    if (ov->BeginMenuBar())
    {
        if (ov->BeginMenu("File"))
        {
            if (ov->MenuItem("Open"))
                ++opened;
            ov->EndMenu();
        }
        ov->EndMenuBar();
    }
    ov->EndWindow();
}

ONYX_TEST(overlay, menus_open_and_activate_items,
          .Description = "A menu bar menu opens on click, its items report clicks, and it closes once one is clicked")
{
    UiEngine ui{};
    u32 opened = 0;
    ui.Gui = [&](Onyx::Overlay *ov) { menuGui(ov, opened); };
    ui.SetRef("Test");
    // the engine takes the careful way into the menu: straight down from the header, a few pixels a frame. a quicker
    // move closes the menu (the next test)
    ui.MenuClick("File/Open");
    CHECK(opened == 1u);
    CHECK_MSG(!ui.ItemExists("##MenuBar/File/Open"), "The menu stayed open after one of its items was clicked");
}

// Fails until fixed: while the bar is open, the hovered header reopens its menu every frame and asks for a collapse to
// its own level, so the menu outlives Draw only while the menu box is hovered too. The box is hovered up to 6 px around
// it (IsHovered applies half of BeginMenu's 12 px padding), which covers just the bottom 6 px of the header: a cursor
// that skips that strip in one frame (over about 6 px a frame, a normal flick at 60 fps) closes the menu. That collapse
// skips closePopup, so the bar also stays open and hovering the header reopens the menu without a click. Both reproduce
// with a real mouse in the overlay demo
ONYX_TEST(overlay, menus_survive_a_quick_move_onto_their_items,
          .Description = "An open menu bar menu stays open when the cursor goes from its header to one of its items "
                         "in a single frame")
{
    UiEngine ui{};
    u32 opened = 0;
    ui.Gui = [&](Onyx::Overlay *ov) { menuGui(ov, opened); };
    ui.SetRef("Test");
    ui.ItemClick("##MenuBar/File");
    REQUIRE_MSG(ui.ItemExists("##MenuBar/File/Open"), "The menu did not open");
    const ItemInfo header = ui.GetItem("##MenuBar/File");
    ui.MouseMove(f32v2{header.Center()[0], header.Min[1] + 2.f}, 1);
    ui.Yield(2);
    REQUIRE_MSG(ui.ItemExists("##MenuBar/File/Open"), "The menu closed while the cursor was on its header");
    ui.MouseMove(ui.GetItem("##MenuBar/File/Open").Center(), 1);
    ui.Yield(2);
    REQUIRE_MSG(ui.ItemExists("##MenuBar/File/Open"), "The menu closed when the cursor went onto its item");
    ui.ItemClick("##MenuBar/File/Open");
    CHECK(opened == 1u);
}

ONYX_TEST(overlay, popups_open_and_close,
          .Description = "OpenPopup shows a popup, CloseCurrentPopup hides it, and a click outside dismisses it")
{
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        if (ov->Button("Open"))
            ov->OpenPopup("Popup");
        if (ov->BeginPopup("Popup"))
        {
            ov->Text("I am a popup");
            if (ov->Button("Close"))
                ov->CloseCurrentPopup();
            ov->EndPopup();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    CHECK(!ui.ItemExists("Popup/Close"));
    ui.ItemClick("Open");
    REQUIRE_MSG(ui.ItemExists("Popup/Close"), "The popup did not open");
    ui.ItemClick("Popup/Close");
    ui.Yield(2);
    CHECK_MSG(!ui.ItemExists("Popup/Close"), "CloseCurrentPopup left it open");

    ui.ItemClick("Open");
    REQUIRE(ui.ItemExists("Popup/Close"));
    ui.MouseClickAt(f32v2{600.f, 450.f});
    ui.Yield(2);
    CHECK_MSG(!ui.ItemExists("Popup/Close"), "A click outside left it open");
}

ONYX_TEST(overlay, context_menus_open_on_right_click,
          .Description = "A context popup opens when its item is right clicked, not on a left click")
{
    UiEngine ui{};
    u32 actions = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->Button("Target");
        if (ov->BeginPopupContextItem("Context"))
        {
            if (ov->Button("Action"))
                ++actions;
            ov->EndPopup();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemClick("Target");
    CHECK_MSG(!ui.ItemExists("Context/Action"), "A left click opened the context popup");
    ui.ItemClick("Target", Onyx::Mouse_ButtonRight);
    REQUIRE_MSG(ui.ItemExists("Context/Action"), "A right click did not open the context popup");
    ui.ItemClick("Context/Action");
    CHECK(actions == 1u);
}

ONYX_TEST(overlay, tooltips_show_while_hovering,
          .Description = "An item tooltip shows while its item is hovered and hides when the mouse leaves")
{
    UiEngine ui{};
    bool shown = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->Button("Hover me");
        shown = ov->SetItemTooltipRaw("A tooltip");
        ov->EndWindow();
    };
    ui.Yield(2);
    CHECK(!shown);
    ui.ItemHover("Test/Hover me");
    // tooltips may wait a moment before they show
    CHECK_MSG(ui.YieldUntil([&] { return shown; }, 120), "The tooltip never showed while hovering");
    ui.MouseMove(f32v2{600.f, 450.f});
    ui.Yield(2);
    CHECK(!shown);
}

ONYX_TEST(overlay, tabs_switch_on_click,
          .Description = "Only the open tab's contents are declared (the StartOpen one at first), and clicking another "
                         "tab's header opens it")
{
    UiEngine ui{};
    bool firstShown = false;
    bool secondShown = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        firstShown = secondShown = false;
        if (!beginTestWindow(ov))
            return;
        ov->BeginTabBar("Tabs");
        if (ov->BeginTab("One", Onyx::OverlayTabFlag_StartOpen))
        {
            firstShown = true;
            ov->EndTab();
        }
        if (ov->BeginTab("Two"))
        {
            secondShown = true;
            ov->EndTab();
        }
        ov->EndTabBar();
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.Yield(2);
    CHECK(firstShown);
    CHECK(!secondShown);
    // "Tabs/Two" is the header of the tab "Two" of the tab bar "Tabs"
    ui.ItemClick("Tabs/Two");
    CHECK(secondShown);
    CHECK(!firstShown);
    ui.ItemClick("Tabs/One");
    CHECK(firstShown);
}

ONYX_TEST(overlay, trees_expand_and_collapse,
          .Description = "A tree node toggles open and closed on click, and its children exist only while open")
{
    UiEngine ui{};
    bool open = false;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        open = ov->PushTree("Node");
        if (open)
        {
            ov->Button("Child");
            ov->PopTree();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.Yield(2);
    CHECK(!open);
    CHECK(!ui.ItemExists("Node/Child"));
    ui.ItemClick("Node");
    CHECK(open);
    CHECK(ui.ItemExists("Node/Child"));
    ui.ItemClick("Node");
    CHECK(!open);
}

ONYX_TEST(overlay, drag_and_drop_delivers_payloads,
          .Description = "A payload dragged from a source and released over a target is accepted by the target")
{
    UiEngine ui{};
    i32 sent = 1234;
    i32 received = 0;
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov))
            return;
        ov->Button("Source");
        if (ov->BeginDragDropSource())
        {
            ov->SetDragDropPayload("INT", &sent);
            ov->Text("Dragging {}", sent);
            ov->EndDragDropSource();
        }
        ov->Button("Target");
        if (ov->BeginDragDropTarget())
        {
            if (const auto payload = ov->AcceptDragDropPayload("INT"))
                received = *static_cast<const i32 *>(payload.Data);
            ov->EndDragDropTarget();
        }
        ov->EndWindow();
    };
    ui.SetRef("Test");
    ui.ItemDragAndDrop("Source", "Target");
    CHECK(received == 1234);
}

// ---------------------------------------------------------------------------------------------------------------------
//  Scenarios: a person going through a whole gui
// ---------------------------------------------------------------------------------------------------------------------

ONYX_TEST(overlay, a_settings_form_is_filled_in_and_applied,
          .Description = "A form with a text input, a numeric input, a checkbox and a dropdown is filled in, and Apply "
                         "hands over everything that was entered")
{
    struct Settings
    {
        char Name[32] = "";
        i32 Age = 0;
        bool Subscribe = false;
        u32 Plan = 0;
    };
    const char *plans[] = {"Free", "Pro", "Team"};
    Settings editing{};
    Settings applied{};
    u32 applies = 0;
    UiEngine ui{};
    ui.Gui = [&](Onyx::Overlay *ov) {
        if (!beginTestWindow(ov, "Settings", {360.f, 300.f}))
            return;
        ov->InputText("Name", editing.Name, sizeof(editing.Name));
        ov->InputNumeric("Age", &editing.Age);
        ov->CheckBox("Subscribe", &editing.Subscribe);
        if (ov->BeginDropDown("Plan", plans[editing.Plan]))
        {
            for (u32 i = 0; i < 3; ++i)
                if (ov->Selectable(plans[i], editing.Plan == i))
                {
                    editing.Plan = i;
                    ov->CloseCurrentPopup();
                }
            ov->EndDropDown();
        }
        if (ov->Button("Apply"))
        {
            applied = editing;
            ++applies;
        }
        ov->EndWindow();
    };
    ui.SetRef("Settings");
    ui.ItemInput("Name", "Ada");
    ui.ItemInput("Age", "36");
    ui.ItemClick("Subscribe");
    ui.ItemClick("Plan");
    ui.ItemClick("Plan/Pro");
    ui.ItemClick("Apply");

    REQUIRE(applies == 1u);
    CHECK(Text(applied.Name) == "Ada");
    CHECK(applied.Age == 36);
    CHECK(applied.Subscribe);
    CHECK_MSG(applied.Plan == 1u, "The plan is '{}'", plans[applied.Plan]);
}
// a menu bar with two menus, "File" (with "Open") and "Edit" (with "Undo")
static void twoMenusGui(Onyx::Overlay *ov, u32 &opened, u32 &undone)
{
    if (!beginTestWindow(ov, "Test", {320.f, 260.f}, Onyx::OverlayWindowFlag_MenuBar))
        return;
    if (ov->BeginMenuBar())
    {
        if (ov->BeginMenu("File"))
        {
            if (ov->MenuItem("Open"))
                ++opened;
            ov->EndMenu();
        }
        if (ov->BeginMenu("Edit"))
        {
            if (ov->MenuItem("Undo"))
                ++undone;
            ov->EndMenu();
        }
        ov->EndMenuBar();
    }
    ov->EndWindow();
}

ONYX_TEST(overlay, hovering_another_header_switches_menus,
          .Description = "With a menu open, hovering another header of the bar opens that one's menu instead")
{
    UiEngine ui{};
    u32 opened = 0;
    u32 undone = 0;
    ui.Gui = [&](Onyx::Overlay *ov) { twoMenusGui(ov, opened, undone); };
    ui.SetRef("Test");
    ui.ItemClick("##MenuBar/File");
    REQUIRE_MSG(ui.ItemExists("##MenuBar/File/Open"), "The File menu did not open");
    ui.ItemHover("##MenuBar/Edit");
    ui.Yield(2);
    CHECK_MSG(ui.ItemExists("##MenuBar/Edit/Undo"), "Hovering Edit did not open its menu");
    CHECK_MSG(!ui.ItemExists("##MenuBar/File/Open"), "The File menu stayed open under the Edit one");
    ui.MenuClick("Edit/Undo");
    CHECK(undone == 1u);
    CHECK(opened == 0u);
}

ONYX_TEST(overlay, menu_bars_close_with_their_menus,
          .Description = "Once a menu item is clicked and the menu closes, hovering a header again does not reopen it "
                         "without a click")
{
    UiEngine ui{};
    u32 opened = 0;
    u32 undone = 0;
    ui.Gui = [&](Onyx::Overlay *ov) { twoMenusGui(ov, opened, undone); };
    ui.SetRef("Test");
    ui.MenuClick("File/Open");
    REQUIRE(opened == 1u);
    ui.MouseMove(f32v2{600.f, 450.f});
    ui.Yield(2);
    ui.ItemHover("##MenuBar/File");
    ui.Yield(3);
    CHECK_MSG(!ui.ItemExists("##MenuBar/File/Open"), "Hovering File reopened its menu without a click");
    ui.ItemHover("##MenuBar/Edit");
    ui.Yield(3);
    CHECK_MSG(!ui.ItemExists("##MenuBar/Edit/Undo"), "Hovering Edit opened its menu without a click");
}
