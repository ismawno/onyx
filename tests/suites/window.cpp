// Windows: what they report, how they follow resizes, present mode changes and close requests, and frame pacing.

#include "harness.hpp"
#include <cstring>

using namespace OnyxTest;

static u32v2 pixelSize(const Onyx::Window *win)
{
    return Graph::Window_GetPixelSize(win->GetHandle());
}
static u32v2 swapchainExtent(const Onyx::Window *win)
{
    return Graph::Swapchain_GetExtent(win->GetSwapchain());
}

ONYX_TEST(window, reports_what_it_was_opened_with,
          .Description = "Title and logical size read back, and the framebuffer is the logical size times the content "
                         "scale")
{
    Onyx::Window *win = OpenTestWindow({.Size = {300, 200}, .Title = "onyx-tests window"});
    CHECK(String{win->GetTitle()} == "onyx-tests window");
    CHECK(win->GetSize() == u32v2(300, 200));

    const f32v2 scale = win->GetContentScale();
    CHECK((scale[0] > 0.f && scale[1] > 0.f));
    CHECK_THAT(scale[0], IsNear(scale[1], 1e-4f));
    CHECK_THAT(f32v2{pixelSize(win)}, IsNear(f32v2{Onyx::ApplyScale(win->GetSize(), scale)}, 1.f));
    CHECK(Onyx::Window::FromHandle(win->GetHandle()) == win);
}

ONYX_TEST(window, title_changes_read_back, .Description = "SetTitle is visible through GetTitle")
{
    Onyx::Window *win = OpenTestWindow();
    win->SetTitle("renamed");
    CHECK(String{win->GetTitle()} == "renamed");
}

ONYX_TEST(window, swapchain_matches_the_framebuffer,
          .Description = "Once rendering, the swapchain has the framebuffer size in physical pixels")
{
    Onyx::Window *win = OpenTestWindow();
    Frame(2);
    REQUIRE(Graph::Swapchain_IsHandleValid(win->GetSwapchain()));
    CHECK(swapchainExtent(win) == pixelSize(win));
    CHECK(win->GetSwapchainImageCount() == Graph::Swapchain_GetImageCount(win->GetSwapchain()));
}

ONYX_TEST(window, resize_recreates_the_swapchain,
          .Description = "After SetSize the window reports the new size, sends a resize event, and the swapchain is "
                         "recreated to match within a few frames")
{
    Onyx::Window *win = OpenTestWindow({.Size = {320, 240}});
    Frame(2);

    const u32v2 target{400, 300};
    win->SetSize(target);
    bool resized = false;
    bool recreated = false;
    const bool settled = WaitFor([&] {
        for (const Onyx::Event &event : win->GetNewEvents())
        {
            resized |= event.Type == Onyx::Event_WindowResized || event.Type == Onyx::Event_FramebufferResized;
            recreated |= event.Type == Onyx::Event_SwapchainRecreated;
        }
        return win->GetSize() == target && swapchainExtent(win) == pixelSize(win);
    });
    CHECK_MSG(settled, "Size {}, framebuffer {}, swapchain {}", win->GetSize(), pixelSize(win), swapchainExtent(win));
    CHECK(resized);
    CHECK(recreated);
}

ONYX_TEST(window, present_modes_switch_cleanly,
          .Description = "Every present mode can be selected at runtime: it reads back and rendering goes on with a "
                         "valid swapchain")
{
    Onyx::Window *win = OpenTestWindow();
    Frame(2);
    for (u32 mode = 0; mode < Graph::PresentMode_Count; ++mode)
    {
        win->SetPresentMode(Graph::PresentMode(mode));
        Frame(3);
        CHECK(u32(win->GetPresentMode()) == mode);
        CHECK(Graph::Swapchain_IsHandleValid(win->GetSwapchain()));
        CHECK(swapchainExtent(win) == pixelSize(win));
    }
}

ONYX_TEST(window, close_request_closes_it,
          .Description = "A close request (the title bar's close button) makes the window close at the end of the "
                         "frame, unless it opted out")
{
    Onyx::Window *win = OpenTestWindow();
    Graph::Window_SignalShouldClose(win->GetHandle());
    CHECK(win->ShouldClose());
    Frame();
    CHECK(!Onyx::Running());

    Onyx::OpenWindowSpecs specs{};
    specs.Window.ScreenSize = {200, 150};
    specs.Flags = Onyx::OpenWindowFlag_DoNotDestroyOnShouldClose | Onyx::OpenWindowFlag_NoDefaultDeltaTime;
    Onyx::Window *kept = Onyx::OpenWindow(specs);
    Graph::Window_SignalShouldClose(kept->GetHandle());
    Frame();
    CHECK(Onyx::Running());
    CHECK(kept->ShouldClose());
}

ONYX_TEST(window, several_windows_are_independent,
          .Description = "Two windows render side by side, and closing one leaves the other running")
{
    Onyx::Window *a = OpenTestWindow({.Title = "onyx-tests a"});
    Onyx::Window *b = OpenTestWindow({.Size = {200, 160}, .Title = "onyx-tests b"});
    Frame(3);
    CHECK(Graph::Swapchain_IsHandleValid(a->GetSwapchain()));
    CHECK(Graph::Swapchain_IsHandleValid(b->GetSwapchain()));
    CHECK(swapchainExtent(b) == pixelSize(b));

    Onyx::CloseWindow(a);
    Frame(2);
    CHECK(Onyx::Running());
    CHECK(swapchainExtent(b) == pixelSize(b));
}

ONYX_TEST(window, target_delta_time_paces_frames,
          .Description = "With a target delta time, frames never come faster than the target (they may come slower)")
{
    Onyx::Window *win = OpenTestWindow();
    const TKit::Timespan target = Onyx::ToDeltaTime(30);
    Onyx::SetTargetDeltaTime(win, target);
    CHECK(Onyx::GetTargetDeltaTime(win) == target);

    Frame(3); // let the pacing settle
    constexpr u32 frames = 15;
    const TKit::Timespan start = Onyx::GetTime();
    Frame(frames);
    const f32 elapsed = (Onyx::GetTime() - start).AsSeconds();
    // one sided: a slow machine stretches frames, but pacing must not let them run faster. 10% leeway for timers
    CHECK_MSG(elapsed >= 0.9f * f32(frames) * target.AsSeconds(), "{} frames took {:.3f} s, target {:.3f} s each",
              frames, elapsed, target.AsSeconds());
}

ONYX_TEST(window, position_reads_back,
          .Description = "SetPosition moves the window where asked (not under a Wayland compositor, which decides it)")
{
    if (CompositorPlacesWindows())
        SKIP("A Wayland compositor places the windows here (XWayland included), not the application");
    Onyx::Window *win = OpenTestWindow();
    const i32v2 target{40, 60};
    win->SetPosition(target);
    const bool moved = WaitFor([&] { return win->GetPosition() == target; });
    CHECK_MSG(moved, "Position {}, asked for {}", win->GetPosition(), target);
}

ONYX_TEST(window, monitors_are_reported,
          .Description = "The platform reports a primary monitor with a positive content scale")
{
    const u32 count = Graph::Monitor_GetCount();
    REQUIRE(count > 0);
    CHECK(Graph::Monitor_IsHandleValid(Graph::Monitor_GetPrimary()));
    for (u32 i = 0; i < count; ++i)
    {
        const f32v2 scale = Graph::Monitor_GetContentScale(Graph::Monitor_Get(i));
        CHECK_MSG(scale[0] > 0.f && scale[1] > 0.f, "Monitor {} has scale {}", i, scale);
    }
}

ONYX_TEST(window, monitor_delta_time_is_usable,
          .Description = "The monitor delta time Onyx derives the default pacing from is positive and finite, even "
                         "when the monitor reports no refresh rate")
{
    Onyx::Window *win = OpenTestWindow();
    const f32 dt = win->GetMonitorDeltaTime().AsSeconds();
    CHECK_MSG(dt > 0.f && dt < 1.f, "Monitor delta time {:.4f} s", dt);
}
