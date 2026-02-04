#pragma once

#include "onyx/core/core.hpp"
#include <functional>

namespace Onyx
{
class Window;

using WindowFlags = u8;
enum WindowFlagBit : WindowFlags
{
    WindowFlag_Resizable = 1 << 0,
    WindowFlag_Visible = 1 << 1,
    WindowFlag_Decorated = 1 << 2,
    WindowFlag_Focused = 1 << 3,
    WindowFlag_Floating = 1 << 4,
};
struct WindowCallbacks
{
    std::function<void(Window *, i32, i32)> WindowPosCallback = nullptr;
    std::function<void(Window *, i32, i32)> WindowSizeCallback = nullptr;
    std::function<void(Window *, i32, i32)> FramebufferSizeCallback = nullptr;
    std::function<void(Window *, i32)> WindowFocusCallback = nullptr;
    std::function<void(Window *)> WindowCloseCallback = nullptr;
    std::function<void(Window *, i32)> WindowIconifyCallback = nullptr;

    std::function<void(Window *, i32, i32, i32, i32)> KeyCallback = nullptr;
    std::function<void(Window *, u32)> CharCallback = nullptr;

    std::function<void(Window *, f64, f64)> CursorPosCallback = nullptr;
    std::function<void(Window *, i32)> CursorEnterCallback = nullptr;
    std::function<void(Window *, i32, i32, i32)> MouseButtonCallback = nullptr;
    std::function<void(Window *, f64, f64)> ScrollCallback = nullptr;
};
struct WindowSpecs
{
    const char *Name = "Onyx window";
    u32v2 Position{TKIT_U32_MAX}; // u32 max means let it be decided automatically
    u32v2 Dimensions{800, 600};
    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    WindowFlags Flags = WindowFlag_Resizable | WindowFlag_Visible | WindowFlag_Decorated | WindowFlag_Focused;
    WindowCallbacks Callbacks{};
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
