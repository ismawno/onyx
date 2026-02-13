#pragma once

#include "onyx/core/core.hpp"

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

} // namespace Onyx::Platform
