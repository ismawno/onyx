// Input through a window: virtual input (WindowFlag_VirtualInput) answers every query from injected events, and what
// reads input through the window (camera controls) follows it. Nothing here needs a real mouse or keyboard.

#include "harness.hpp"
#include <iterator>

using namespace OnyxTest;

static Onyx::Window *openVirtualWindow()
{
    return OpenTestWindow({.Size = {400, 300}, .Flags = Onyx::WindowFlag_VirtualInput});
}

ONYX_TEST(input, virtual_mouse_follows_injected_moves,
          .Description = "With virtual input the mouse position and hover state come from injected moves, in logical "
                         "pixels")
{
    Onyx::Window *win = openVirtualWindow();
    REQUIRE(win->HasVirtualInput());
    // until it enters, the mouse is outside, as if it had not been over the window yet
    CHECK(!win->IsHovered());

    win->InjectMouseMove({123.f, 45.f});
    CHECK_THAT(win->GetMousePosition(), IsNear(f32v2(123.f, 45.f), 1e-3f));
    CHECK(win->IsHovered());

    win->InjectMouseMove({450.f, 45.f});
    CHECK(!win->IsHovered());
}

ONYX_TEST(input, virtual_keys_and_buttons_track_events,
          .Description = "Key and button queries follow injected presses and releases, repeats included")
{
    Onyx::Window *win = openVirtualWindow();
    CHECK(win->IsKeyReleased(Onyx::Key_A));

    win->InjectKey(Onyx::Key_A, true);
    CHECK(win->IsKeyPressed(Onyx::Key_A));
    CHECK(!win->IsKeyPressed(Onyx::Key_B));
    win->InjectKey(Onyx::Key_A, true, true);
    CHECK(win->IsKeyPressed(Onyx::Key_A));
    win->InjectKey(Onyx::Key_A, false);
    CHECK(win->IsKeyReleased(Onyx::Key_A));

    win->InjectMouseButton(Onyx::Mouse_ButtonRight, true);
    CHECK(win->IsMousePressed(Onyx::Mouse_ButtonRight));
    CHECK(win->IsMouseReleased(Onyx::Mouse_ButtonLeft));
    win->InjectMouseButton(Onyx::Mouse_ButtonRight, false);
    CHECK(win->IsMouseReleased(Onyx::Mouse_ButtonRight));
}

ONYX_TEST(input, injected_events_reach_the_event_stream,
          .Description = "Injected input shows up in GetNewEvents in order, with the crossing events a platform would "
                         "send, and is gone after the next render")
{
    Onyx::Window *win = openVirtualWindow();
    Frame(1);
    Onyx::Running();
    win->FlushEvents(); // only what this test injects

    win->InjectMouseMove({10.f, 10.f});
    win->InjectMouseButton(Onyx::Mouse_ButtonLeft, true);
    win->InjectMouseButton(Onyx::Mouse_ButtonLeft, false);
    win->InjectScroll({0.f, -2.f});
    win->InjectKey(Onyx::Key_Enter, true);
    win->InjectChar('x');
    win->InjectMouseMove({-5.f, 10.f});

    const Onyx::EventType expected[] = {Onyx::Event_MouseMoved,    Onyx::Event_MouseEntered, Onyx::Event_MousePressed,
                                        Onyx::Event_MouseReleased, Onyx::Event_Scrolled,     Onyx::Event_KeyPressed,
                                        Onyx::Event_CharInput,     Onyx::Event_MouseMoved,   Onyx::Event_MouseLeft};
    const auto &events = win->GetNewEvents();
    REQUIRE(events.GetSize() == u32(std::size(expected)));
    for (u32 i = 0; i < events.GetSize(); ++i)
        CHECK_MSG(events[i].Type == expected[i], "Event {} has type {}, expected {}", i, u32(events[i].Type),
                  u32(expected[i]));
    CHECK(events[2].Mouse.Button == Onyx::Mouse_ButtonLeft);
    CHECK_THAT(events[4].ScrollOffset, IsNear(f32v2(0.f, -2.f), 1e-6f));
    CHECK(events[5].Key == Onyx::Key_Enter);
    CHECK(events[6].Character == Onyx::CodePoint('x'));

    Onyx::Transfer();
    Onyx::Render();
    CHECK(win->GetNewEvents().IsEmpty());
}

ONYX_TEST(input, camera_controls_move_a_2d_camera,
          .Description = "ControlCamera moves and rotates a 2D camera while its keys are held")
{
    Onyx::Window *win = openVirtualWindow();
    Onyx::Camera<D2> cam{};
    const TKit::Timespan dt = TKit::Timespan::FromSeconds(0.1f);

    win->InjectKey(Onyx::Key_W, true);
    win->ControlCamera(dt, &cam);
    CHECK_MSG(cam.View.Translation[1] > 0.f, "W moved the camera to {}", cam.View.Translation);
    win->InjectKey(Onyx::Key_W, false);

    const f32v2 before = cam.View.Translation;
    win->ControlCamera(dt, &cam);
    CHECK_THAT(cam.View.Translation, IsNear(before, 1e-6f));

    win->InjectKey(Onyx::Key_D, true);
    win->ControlCamera(dt, &cam);
    CHECK(cam.View.Translation[0] > before[0]);
    win->InjectKey(Onyx::Key_D, false);

    win->InjectKey(Onyx::Key_Q, true);
    win->ControlCamera(dt, &cam);
    CHECK(cam.View.Rotation != 0.f);
}

ONYX_TEST(input, camera_controls_move_a_3d_camera,
          .Description = "ControlCamera moves a 3D camera forward along its view direction and up with Space")
{
    Onyx::Window *win = openVirtualWindow();
    Onyx::Camera<D3> cam{};
    const TKit::Timespan dt = TKit::Timespan::FromSeconds(0.1f);

    win->InjectKey(Onyx::Key_W, true);
    win->ControlCamera(dt, &cam);
    win->InjectKey(Onyx::Key_W, false);
    // the default camera looks down -Z
    CHECK_MSG(cam.View.Translation[2] < 0.f, "W moved the camera to {}", cam.View.Translation);

    const f32 y = cam.View.Translation[1];
    win->InjectKey(Onyx::Key_Space, true);
    win->ControlCamera(dt, &cam);
    win->InjectKey(Onyx::Key_Space, false);
    CHECK(cam.View.Translation[1] > y);
}
