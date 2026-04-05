#pragma once

#include "onyx/core/core.hpp"
#include "tkit/math/tensor.hpp"

#define ONYX_PLATFORM_ANY 0x00060000
#define ONYX_PLATFORM_WIN32 0x00060001
#define ONYX_PLATFORM_COCOA 0x00060002
#define ONYX_PLATFORM_WAYLAND 0x00060003
#define ONYX_PLATFORM_X11 0x00060004
#define ONYX_PLATFORM_NULL 0x00060005

#ifdef TKIT_OS_LINUX
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_X11
#elif defined(TKIT_OS_APPLE)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_COCOA
#elif defined(TKIT_OS_WINDOWS)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_WIN32
#endif

#ifndef ONYX_PLATFORM_AUTO
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_ANY
#endif

namespace Onyx
{
class Window;

using WindowFlags = u16;
enum WindowFlagBit : WindowFlags
{
    WindowFlag_Resizable = 1 << 0,
    WindowFlag_Visible = 1 << 1,
    WindowFlag_Decorated = 1 << 2,
    WindowFlag_Focused = 1 << 3,
    WindowFlag_Floating = 1 << 4,
    WindowFlag_FocusOnShow = 1 << 5,
    WindowFlag_Iconified = 1 << 6,
    WindowFlag_InstallCallbacks = 1 << 7,
};

struct WindowSpecs
{
    const char *Title = "Onyx window";
    i32v2 Position{TKIT_I32_MAX}; // i32 max means let it be decided automatically
    u32v2 Dimensions{800, 600};
    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    WindowFlags Flags = WindowFlag_Resizable | WindowFlag_Visible | WindowFlag_Decorated | WindowFlag_Focused |
                        WindowFlag_InstallCallbacks;
};

} // namespace Onyx

namespace Onyx::Platform
{
struct Specs
{
    u32 Platform = ONYX_PLATFORM_AUTO;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

ONYX_NO_DISCARD Result<Window *> CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(Window *window);
void DestroyWindows();
void PollEvents();

} // namespace Onyx::Platform
