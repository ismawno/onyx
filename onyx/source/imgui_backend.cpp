#include "pch.hpp"
#include "imgui_backend.hpp"
#include "core.hpp"
#include "graph/execution.hpp"
#include "graph/descriptor.hpp"
#include "graph/pipeline.hpp"
#include "graph/shader.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/utils/debug.hpp"
#include <imgui_internal.h>

#if !defined(ONYX_IMGUI_DISABLE_X11) && (defined(TKIT_OS_LINUX) || defined(__FreeBSD__) || defined(__OpenBSD__) ||     \
                                         defined(__NetBSD__) || defined(__DragonFly__))
#    define ONYX_IMGUI_X11
#endif

#if !defined(ONYX_IMGUI_DISABLE_WAYLAND) && (defined(TKIT_OS_LINUX) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
                                             defined(__NetBSD__) || defined(__DragonFly__))
#    define ONYX_IMGUI_WAYLAND
#endif

namespace ImGui
{
extern ImGuiIO &GetIO(ImGuiContext *);
} // namespace ImGui

namespace Onyx
{
struct Platform_GlobalData
{
    TKit::FixedArray<Graph::MouseCursor, ImGuiMouseCursor_COUNT> MouseCursors;
#ifdef ONYX_IMGUI_WAYLAND
    bool IsWayland;
#endif
};
static Platform_GlobalData s_PlatformData{};

struct Platform_ContextData
{
    ImGuiContext *Context;
    Window *Window;

    Graph::Window MouseWindow = Graph::NullHandle;
    TKit::FixedArray<Graph::Window, Graph::Key_Count> KeyOwnerWindows;

    TKit::Clock Clock{};
    f64 Time = 0.f;

    bool MouseIgnoreButtonUpWaitForFocusLoss = false;
    bool MouseIgnoreButtonUp = false;

    f32v2 LastValidMousePos{0.f};

    Graph::WindowFocusCallback UserCbkWindowFocus;
    Graph::WindowCursorPosCallback UserCbkCursorPos;
    Graph::WindowCursorEnterCallback UserCbkCursorEnter;
    Graph::WindowMouseButtonCallback UserCbkMousebutton;
    Graph::WindowScrollCallback UserCbkScroll;
    Graph::WindowKeyCallback UserCbkKey;
    Graph::WindowCharCallback UserCbkChar;
};

struct Platform_ViewportData
{
    Window *Window = nullptr;
    u32 IgnoreWindowPosEventFrame = 0;
    u32 IgnoreWindowSizeEventFrame = 0;
    Graph::MouseCursor LastMouseCursor = Graph::MouseCursor_Count; // Count means the OS cursor is hidden
};

ImGuiKey platform_KeyToImGuiKey(const Graph::Key key)
{
    switch (key)
    {
    case Graph::Key_Tab:
        return ImGuiKey_Tab;
    case Graph::Key_Left:
        return ImGuiKey_LeftArrow;
    case Graph::Key_Right:
        return ImGuiKey_RightArrow;
    case Graph::Key_Up:
        return ImGuiKey_UpArrow;
    case Graph::Key_Down:
        return ImGuiKey_DownArrow;
    case Graph::Key_PageUp:
        return ImGuiKey_PageUp;
    case Graph::Key_PageDown:
        return ImGuiKey_PageDown;
    case Graph::Key_Home:
        return ImGuiKey_Home;
    case Graph::Key_End:
        return ImGuiKey_End;
    case Graph::Key_Insert:
        return ImGuiKey_Insert;
    case Graph::Key_Delete:
        return ImGuiKey_Delete;
    case Graph::Key_Backspace:
        return ImGuiKey_Backspace;
    case Graph::Key_Space:
        return ImGuiKey_Space;
    case Graph::Key_Enter:
        return ImGuiKey_Enter;
    case Graph::Key_Escape:
        return ImGuiKey_Escape;
    case Graph::Key_Apostrophe:
        return ImGuiKey_Apostrophe;
    case Graph::Key_Comma:
        return ImGuiKey_Comma;
    case Graph::Key_Minus:
        return ImGuiKey_Minus;
    case Graph::Key_Period:
        return ImGuiKey_Period;
    case Graph::Key_Slash:
        return ImGuiKey_Slash;
    case Graph::Key_Semicolon:
        return ImGuiKey_Semicolon;
    case Graph::Key_Equal:
        return ImGuiKey_Equal;
    case Graph::Key_LeftBracket:
        return ImGuiKey_LeftBracket;
    case Graph::Key_Backslash:
        return ImGuiKey_Backslash;
    case Graph::Key_World_1:
    case Graph::Key_World_2:
        return ImGuiKey_Oem102;
    case Graph::Key_RightBracket:
        return ImGuiKey_RightBracket;
    case Graph::Key_GraveAccent:
        return ImGuiKey_GraveAccent;
    case Graph::Key_CapsLock:
        return ImGuiKey_CapsLock;
    case Graph::Key_ScrollLock:
        return ImGuiKey_ScrollLock;
    case Graph::Key_NumLock:
        return ImGuiKey_NumLock;
    case Graph::Key_PrintScreen:
        return ImGuiKey_PrintScreen;
    case Graph::Key_Pause:
        return ImGuiKey_Pause;
    case Graph::Key_KP_0:
        return ImGuiKey_Keypad0;
    case Graph::Key_KP_1:
        return ImGuiKey_Keypad1;
    case Graph::Key_KP_2:
        return ImGuiKey_Keypad2;
    case Graph::Key_KP_3:
        return ImGuiKey_Keypad3;
    case Graph::Key_KP_4:
        return ImGuiKey_Keypad4;
    case Graph::Key_KP_5:
        return ImGuiKey_Keypad5;
    case Graph::Key_KP_6:
        return ImGuiKey_Keypad6;
    case Graph::Key_KP_7:
        return ImGuiKey_Keypad7;
    case Graph::Key_KP_8:
        return ImGuiKey_Keypad8;
    case Graph::Key_KP_9:
        return ImGuiKey_Keypad9;
    case Graph::Key_KPDecimal:
        return ImGuiKey_KeypadDecimal;
    case Graph::Key_KPDivide:
        return ImGuiKey_KeypadDivide;
    case Graph::Key_KPMultiply:
        return ImGuiKey_KeypadMultiply;
    case Graph::Key_KPSubtract:
        return ImGuiKey_KeypadSubtract;
    case Graph::Key_KPAdd:
        return ImGuiKey_KeypadAdd;
    case Graph::Key_KPEnter:
        return ImGuiKey_KeypadEnter;
    case Graph::Key_KPEqual:
        return ImGuiKey_KeypadEqual;
    case Graph::Key_LeftShift:
        return ImGuiKey_LeftShift;
    case Graph::Key_LeftControl:
        return ImGuiKey_LeftCtrl;
    case Graph::Key_LeftAlt:
        return ImGuiKey_LeftAlt;
    case Graph::Key_LeftSuper:
        return ImGuiKey_LeftSuper;
    case Graph::Key_RightShift:
        return ImGuiKey_RightShift;
    case Graph::Key_RightControl:
        return ImGuiKey_RightCtrl;
    case Graph::Key_RightAlt:
        return ImGuiKey_RightAlt;
    case Graph::Key_RightSuper:
        return ImGuiKey_RightSuper;
    case Graph::Key_Menu:
        return ImGuiKey_Menu;
    case Graph::Key_N0:
        return ImGuiKey_0;
    case Graph::Key_N1:
        return ImGuiKey_1;
    case Graph::Key_N2:
        return ImGuiKey_2;
    case Graph::Key_N3:
        return ImGuiKey_3;
    case Graph::Key_N4:
        return ImGuiKey_4;
    case Graph::Key_N5:
        return ImGuiKey_5;
    case Graph::Key_N6:
        return ImGuiKey_6;
    case Graph::Key_N7:
        return ImGuiKey_7;
    case Graph::Key_N8:
        return ImGuiKey_8;
    case Graph::Key_N9:
        return ImGuiKey_9;
    case Graph::Key_A:
        return ImGuiKey_A;
    case Graph::Key_B:
        return ImGuiKey_B;
    case Graph::Key_C:
        return ImGuiKey_C;
    case Graph::Key_D:
        return ImGuiKey_D;
    case Graph::Key_E:
        return ImGuiKey_E;
    case Graph::Key_F:
        return ImGuiKey_F;
    case Graph::Key_G:
        return ImGuiKey_G;
    case Graph::Key_H:
        return ImGuiKey_H;
    case Graph::Key_I:
        return ImGuiKey_I;
    case Graph::Key_J:
        return ImGuiKey_J;
    case Graph::Key_K:
        return ImGuiKey_K;
    case Graph::Key_L:
        return ImGuiKey_L;
    case Graph::Key_M:
        return ImGuiKey_M;
    case Graph::Key_N:
        return ImGuiKey_N;
    case Graph::Key_O:
        return ImGuiKey_O;
    case Graph::Key_P:
        return ImGuiKey_P;
    case Graph::Key_Q:
        return ImGuiKey_Q;
    case Graph::Key_R:
        return ImGuiKey_R;
    case Graph::Key_S:
        return ImGuiKey_S;
    case Graph::Key_T:
        return ImGuiKey_T;
    case Graph::Key_U:
        return ImGuiKey_U;
    case Graph::Key_V:
        return ImGuiKey_V;
    case Graph::Key_W:
        return ImGuiKey_W;
    case Graph::Key_X:
        return ImGuiKey_X;
    case Graph::Key_Y:
        return ImGuiKey_Y;
    case Graph::Key_Z:
        return ImGuiKey_Z;
    case Graph::Key_F1:
        return ImGuiKey_F1;
    case Graph::Key_F2:
        return ImGuiKey_F2;
    case Graph::Key_F3:
        return ImGuiKey_F3;
    case Graph::Key_F4:
        return ImGuiKey_F4;
    case Graph::Key_F5:
        return ImGuiKey_F5;
    case Graph::Key_F6:
        return ImGuiKey_F6;
    case Graph::Key_F7:
        return ImGuiKey_F7;
    case Graph::Key_F8:
        return ImGuiKey_F8;
    case Graph::Key_F9:
        return ImGuiKey_F9;
    case Graph::Key_F10:
        return ImGuiKey_F10;
    case Graph::Key_F11:
        return ImGuiKey_F11;
    case Graph::Key_F12:
        return ImGuiKey_F12;
    case Graph::Key_F13:
        return ImGuiKey_F13;
    case Graph::Key_F14:
        return ImGuiKey_F14;
    case Graph::Key_F15:
        return ImGuiKey_F15;
    case Graph::Key_F16:
        return ImGuiKey_F16;
    case Graph::Key_F17:
        return ImGuiKey_F17;
    case Graph::Key_F18:
        return ImGuiKey_F18;
    case Graph::Key_F19:
        return ImGuiKey_F19;
    case Graph::Key_F20:
        return ImGuiKey_F20;
    case Graph::Key_F21:
        return ImGuiKey_F21;
    case Graph::Key_F22:
        return ImGuiKey_F22;
    case Graph::Key_F23:
        return ImGuiKey_F23;
    case Graph::Key_F24:
        return ImGuiKey_F24;
    default: // Key_F25 has no ImGui counterpart
        return ImGuiKey_None;
    }
}

struct WindowDataPair
{
    Graph::Window Window;
    Platform_ContextData *Data;
};
static TKit::TierArray<WindowDataPair> s_PlatformMap{};

static Platform_ContextData *platform_MapGet(const Graph::Window window)
{
    for (const WindowDataPair &dp : s_PlatformMap)
        if (dp.Window == window)
            return dp.Data;
    TKIT_FATAL("[ONYX][IMGUI] Platform data not found for window");
    return nullptr;
}

static void platform_MapAdd(const Graph::Window window, Platform_ContextData *pdata)
{
    s_PlatformMap.Append(window, pdata);
}

static void platform_MapRemove(const Graph::Window window)
{
    for (u32 i = 0; i < s_PlatformMap.GetSize(); ++i)
    {
        const WindowDataPair &pair = s_PlatformMap[i];
        if (pair.Window == window)
        {
            s_PlatformMap.RemoveUnordered(s_PlatformMap.begin() + i);
            return;
        }
    }
    TKIT_FATAL("[ONYX][IMGUI] Platform data not found for window");
}

static Platform_ContextData *platform_GetContextData(const Graph::Window window = Graph::NullHandle)
{
    if (window != Graph::NullHandle)
        return platform_MapGet(window);

    return ImGui::GetCurrentContext() ? scast<Platform_ContextData *>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
}

static Platform_ViewportData *platform_GetViewportData(const ImGuiViewport *viewport)
{
    return scast<Platform_ViewportData *>(viewport->PlatformUserData);
}

static void platform_WindowFocusCallback(const Graph::Window window, const bool focused)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkWindowFocus && window == pdata->Window->GetHandle())
        pdata->UserCbkWindowFocus(window, focused);

    // Workaround for Linux: when losing focus with MouseIgnoreButtonUpWaitForFocusLoss set, we will temporarily ignore
    // subsequent Mouse Up events
    pdata->MouseIgnoreButtonUp = (pdata->MouseIgnoreButtonUpWaitForFocusLoss && !focused);
    pdata->MouseIgnoreButtonUpWaitForFocusLoss = false;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddFocusEvent(focused);
}

static void platform_CursorPosCallback(const Graph::Window window, f32v2 pos)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkCursorPos && window == pdata->Window->GetHandle())
        pdata->UserCbkCursorPos(window, pos);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        const i32v2 wpos = Graph::Window_GetPosition(window);
        pos += f32v2{wpos};
    }

    io.AddMousePosEvent(pos[0], pos[1]);
    pdata->LastValidMousePos = pos;
}

// Workaround: X11 seems to send spurious Leave/Enter events which would make us lose our position,
// so we back it up and restore on Leave/Enter (see https://github.com/ocornut/imgui/issues/4984)
static void platform_CursorEnterCallback(const Graph::Window window, const bool entered)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkCursorEnter && window == pdata->Window->GetHandle())
        pdata->UserCbkCursorEnter(window, entered);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    if (entered)
    {
        pdata->MouseWindow = window;
        io.AddMousePosEvent(pdata->LastValidMousePos[0], pdata->LastValidMousePos[1]);
    }
    else if (!entered && pdata->MouseWindow == window)
    {
        pdata->LastValidMousePos = f32v2{io.MousePos.x, io.MousePos.y};
        pdata->MouseWindow = Graph::NullHandle;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

static void platform_UpdateKeyModifiers(ImGuiIO &io, const Graph::Window window)
{
    io.AddKeyEvent(ImGuiMod_Ctrl, Graph::Window_IsKeyPressed(window, Graph::Key_LeftControl) ||
                                      Graph::Window_IsKeyPressed(window, Graph::Key_RightControl));
    io.AddKeyEvent(ImGuiMod_Shift, Graph::Window_IsKeyPressed(window, Graph::Key_LeftShift) ||
                                       Graph::Window_IsKeyPressed(window, Graph::Key_RightShift));
    io.AddKeyEvent(ImGuiMod_Alt, Graph::Window_IsKeyPressed(window, Graph::Key_LeftAlt) ||
                                     Graph::Window_IsKeyPressed(window, Graph::Key_RightAlt));
    io.AddKeyEvent(ImGuiMod_Super, Graph::Window_IsKeyPressed(window, Graph::Key_LeftSuper) ||
                                       Graph::Window_IsKeyPressed(window, Graph::Key_RightSuper));
}
static void platform_MouseButtonCallback(const Graph::Window window, const Graph::Mouse button,
                                         const Graph::InputAction action, const Graph::KeyModFlags mods)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkMousebutton && window == pdata->Window->GetHandle())
        pdata->UserCbkMousebutton(window, button, action, mods);

    // Workaround for Linux: ignore mouse up events which are following an focus loss following a viewport creation
    if (pdata->MouseIgnoreButtonUp && action == Graph::InputAction_Release)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    platform_UpdateKeyModifiers(io, window);
    // Mouse_Button1 -> Mouse_Button5 line up with ImGui's left, right, middle, back and forward buttons
    if (i32(button) < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(i32(button), action == Graph::InputAction_Press);
}

static void platform_ScrollCallback(const Graph::Window window, const f32v2 offset)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkScroll && window == pdata->Window->GetHandle())
        pdata->UserCbkScroll(window, offset);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddMouseWheelEvent(offset[0], offset[1]);
}

static Graph::Key platform_TranslateUntranslatedKey(Graph::Key key, const i32 scancode)
{
    // GLFW 3.1+ attempts to "untranslate" keys, which goes the opposite of what every other framework does, making
    // using lettered shortcuts difficult. (It had reasons to do so: namely GLFW is/was more likely to be used for
    // WASD-type game controls rather than lettered shortcuts, but IHMO the 3.1 change could have been done differently)
    // See https://github.com/glfw/glfw/issues/1502 for details.
    // Adding a workaround to undo this (so our keys are translated->untranslated->translated, likely a lossy process).
    // This won't cover edge cases but this is at least going to cover common cases.
    if (key >= Graph::Key_KP_0 && key <= Graph::Key_KPEqual)
        return key;

    const char *keyName = Graph::Key_GetName(key, scancode);
    if (keyName && keyName[0] != 0 && keyName[1] == 0)
    {
        const char charNames[] = "`-=[]\\,;\'./";
        const Graph::Key charKeys[] = {Graph::Key_GraveAccent, Graph::Key_Minus,        Graph::Key_Equal,
                                       Graph::Key_LeftBracket, Graph::Key_RightBracket, Graph::Key_Backslash,
                                       Graph::Key_Comma,       Graph::Key_Semicolon,    Graph::Key_Apostrophe,
                                       Graph::Key_Period,      Graph::Key_Slash,        Graph::Key_None};

        if (keyName[0] >= '0' && keyName[0] <= '9')
            key = Graph::Key(Graph::Key_N0 + (keyName[0] - '0'));
        else if (keyName[0] >= 'A' && keyName[0] <= 'Z')
            key = Graph::Key(Graph::Key_A + (keyName[0] - 'A'));
        else if (keyName[0] >= 'a' && keyName[0] <= 'z')
            key = Graph::Key(Graph::Key_A + (keyName[0] - 'a'));
        else if (const char *p = strchr(charNames, keyName[0]))
            key = charKeys[p - charNames];
    }
    return key;
}

static void platform_KeyCallback(const Graph::Window window, Graph::Key key, const i32 scancode,
                                 const Graph::InputAction action, const Graph::KeyModFlags mods)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkKey && window == pdata->Window->GetHandle())
        pdata->UserCbkKey(window, key, scancode, action, mods);

    if (action != Graph::InputAction_Press && action != Graph::InputAction_Release)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    platform_UpdateKeyModifiers(io, window);

    if (key < Graph::Key_Count)
        pdata->KeyOwnerWindows[key] = (action == Graph::InputAction_Press) ? window : Graph::NullHandle;

    key = platform_TranslateUntranslatedKey(key, scancode);
    const ImGuiKey imkey = platform_KeyToImGuiKey(key);
    io.AddKeyEvent(imkey, (action == Graph::InputAction_Press));
    io.SetKeyEventNativeData(imkey, i32(key), scancode); // To support legacy indexing (<1.87 user code)
}

static void platform_CharCallback(const Graph::Window window, const u32 code)
{
    Platform_ContextData *pdata = platform_GetContextData(window);
    if (pdata->UserCbkChar && window == pdata->Window->GetHandle())
        pdata->UserCbkChar(window, code);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddInputCharacter(code);
}

static ImGuiViewport *platform_FindViewportByPlatformHandle(const Graph::Window window)
{
    if (!ImGui::GetCurrentContext())
        return nullptr;
    return ImGui::FindViewportByPlatformHandle(Window::FromHandle(window));
}

static void platform_WindowCloseCallback(const Graph::Window window)
{
    if (ImGuiViewport *viewport = platform_FindViewportByPlatformHandle(window))
        viewport->PlatformRequestClose = true;
}
static void platform_WindowPosCallback(const Graph::Window window, const i32v2)
{
    if (ImGuiViewport *viewport = platform_FindViewportByPlatformHandle(window))
    {
        Platform_ViewportData *vdata = platform_GetViewportData(viewport);
        TKIT_ASSERT(vdata, "[ONYX][IMGUI] Platform viewport data is null. Consider checking instead of asserting");

        const u32 frameCount = u32(ImGui::GetFrameCount());
        if (frameCount > vdata->IgnoreWindowPosEventFrame + 1)
            viewport->PlatformRequestMove = true;
    }
}
static void platform_WindowSizeCallback(const Graph::Window window, const u32v2)
{
    if (ImGuiViewport *viewport = platform_FindViewportByPlatformHandle(window))
    {
        Platform_ViewportData *vdata = platform_GetViewportData(viewport);
        TKIT_ASSERT(vdata, "[ONYX][IMGUI] Platform viewport data is null. Consider checking instead of asserting");

        const u32 frameCount = u32(ImGui::GetFrameCount());
        if (frameCount > vdata->IgnoreWindowSizeEventFrame + 1)
            viewport->PlatformRequestResize = true;
    }
}

static void platform_InstallCallbacks(Platform_ContextData *pdata, const Graph::Window window)
{
    pdata->UserCbkWindowFocus = Graph::Window_Callback_Focus(window, platform_WindowFocusCallback);
    pdata->UserCbkCursorEnter = Graph::Window_Callback_CursorEnter(window, platform_CursorEnterCallback);
    pdata->UserCbkCursorPos = Graph::Window_Callback_CursorPos(window, platform_CursorPosCallback);
    pdata->UserCbkMousebutton = Graph::Window_Callback_MouseButton(window, platform_MouseButtonCallback);
    pdata->UserCbkScroll = Graph::Window_Callback_Scroll(window, platform_ScrollCallback);
    pdata->UserCbkKey = Graph::Window_Callback_Key(window, platform_KeyCallback);
    pdata->UserCbkChar = Graph::Window_Callback_Char(window, platform_CharCallback);
}

f32 platform_GetContentScaleForMonitor(const Graph::Monitor monitor)
{
#ifdef ONYX_IMGUI_WAYLAND
    if (s_PlatformData.IsWayland)
        return 1.f;
#endif
#if !(defined(TKIT_OS_APPLE) || defined(TKIT_OS_ANDROID))
    return Graph::Monitor_GetContentScale(monitor)[0];
#else
    TKIT_UNUSED(monitor);
    return 1.f;
#endif
}

static void platform_UpdateMonitors()
{
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

    const u32 mcount = Graph::Monitor_GetCount();

    bool updated_monitors = false;
    for (u32 n = 0; n < mcount; n++)
    {
        const Graph::Monitor mon = Graph::Monitor_Get(n);

        ImGuiPlatformMonitor monitor;
        const i32v2 pos = Graph::Monitor_GetPosition(mon);
        const Graph::VideoMode vmode = Graph::Monitor_GetVideoMode(mon);
        if (vmode.Size[0] == 0 || vmode.Size[1] == 0)
            continue;

        monitor.MainPos = monitor.WorkPos = ImVec2(f32(pos[0]), f32(pos[1]));
        monitor.MainSize = monitor.WorkSize = ImVec2(f32(vmode.Size[0]), f32(vmode.Size[1]));

        const Graph::MonitorArea work = Graph::Monitor_GetWorkArea(mon);
        if (work.Size[0] > 0 && work.Size[1] > 0)
        {
            monitor.WorkPos = ImVec2(f32(work.Position[0]), f32(work.Position[1]));
            monitor.WorkSize = ImVec2(f32(work.Size[0]), f32(work.Size[1]));
        }
        const f32 scale = platform_GetContentScaleForMonitor(mon);
        if (Math::ApproachesZero(scale))
            continue;
        monitor.DpiScale = scale;
        monitor.PlatformHandle = rcast<void *>(usz(mon)); // Monitor handles are plain integers

        if (!updated_monitors)
            pio.Monitors.resize(0);
        updated_monitors = true;
        pio.Monitors.push_back(monitor);
    }
}

static void platform_GetWindowSizeAndFramebufferScale(const Graph::Window window, ImVec2 *size, ImVec2 *fbscale)
{
    const u32v2 wdim = Graph::Window_GetScreenSize(window);
    const u32v2 fdim = Graph::Window_GetPixelSize(window);

    f32 fbscx = (wdim[0] > 0) ? f32(fdim[0]) / f32(wdim[0]) : 1.f;
    f32 fbscy = (wdim[1] > 0) ? f32(fdim[1]) / f32(wdim[1]) : 1.f;
#ifdef ONYX_IMGUI_WAYLAND
    if (!s_PlatformData.IsWayland)
    {
        fbscx = 1.f;
        fbscy = 1.f;
    }
#endif
    if (size)
        *size = ImVec2(f32(wdim[0]), f32(wdim[1]));
    if (fbscale)
        *fbscale = ImVec2(fbscx, fbscy);
}

static Platform_ViewportData *platform_CreateViewportData(Window *window)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Platform_ViewportData *vdata = tier->Create<Platform_ViewportData>();
    vdata->Window = window;
    return vdata;
}

static void platform_DestroyViewportData(const Platform_ViewportData *vdata)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(vdata);
}

static void platform_CreateWindow(ImGuiViewport *viewport)
{
    Platform_ContextData *pdata = platform_GetContextData();

#ifdef TKIT_OS_LINUX
    pdata->MouseIgnoreButtonUpWaitForFocusLoss = true;
#endif

    WindowSpecs specs{};
    specs.Flags = 0;
    if (viewport->Flags & ImGuiViewportFlags_NoDecoration)
        specs.Flags |= WindowFlag_NoDecoration;
    if (viewport->Flags & ImGuiViewportFlags_TopMost)
        specs.Flags |= WindowFlag_Floating;

    specs.PresentMode = pdata->Window->GetPresentMode();
    specs.ScreenSize = u32v2{viewport->Size.x, viewport->Size.y};

    Platform_ViewportData *vdata = platform_CreateViewportData(CreateWindow(specs));
    TKIT_LOG_DEBUG("[ONYX][IMGUI] Creating platform window '{}'", vdata->Window->GetTitle());

    viewport->PlatformUserData = vdata;
    viewport->PlatformHandle = vdata->Window;

    const Graph::Window handle = vdata->Window->GetHandle();
    platform_MapAdd(handle, pdata);

    Graph::Window_SetPosition(handle, i32v2{i32(viewport->Pos.x), i32(viewport->Pos.y)});
    Graph::Window_Callback_Focus(handle, platform_WindowFocusCallback);
    Graph::Window_Callback_CursorEnter(handle, platform_CursorEnterCallback);
    Graph::Window_Callback_CursorPos(handle, platform_CursorPosCallback);
    Graph::Window_Callback_MouseButton(handle, platform_MouseButtonCallback);
    Graph::Window_Callback_Scroll(handle, platform_ScrollCallback);
    Graph::Window_Callback_Key(handle, platform_KeyCallback);
    Graph::Window_Callback_Char(handle, platform_CharCallback);
    Graph::Window_Callback_Close(handle, platform_WindowCloseCallback);
    Graph::Window_Callback_Position(handle, platform_WindowPosCallback);
    Graph::Window_Callback_Size(handle, platform_WindowSizeCallback);
}

static void platform_DestroyWindow(ImGuiViewport *viewport)
{
    Platform_ContextData *pdata = platform_GetContextData();
    Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    TKIT_LOG_WARNING_IF(!vdata, "[ONYX][IMGUI] Platform viewport data is null when destroying platform window");

    if (vdata)
    {
        TKIT_LOG_DEBUG("[ONYX][IMGUI] Destroying platform window '{}'", vdata->Window->GetTitle());

        const Graph::Window handle = vdata->Window->GetHandle();
        for (u32 i = 0; i < pdata->KeyOwnerWindows.GetSize(); ++i)
            if (pdata->KeyOwnerWindows[i] == handle)
                platform_KeyCallback(handle, Graph::Key(i), 0, Graph::InputAction_Release, 0);

        platform_MapRemove(handle);

        DestroyWindow(vdata->Window);
        platform_DestroyViewportData(vdata);
    }

    viewport->PlatformUserData = nullptr;
    viewport->PlatformHandle = nullptr;
}

static i32v2 platform_GetWindowPos(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    return Graph::Window_GetPosition(vdata->Window->GetHandle());
}

static void platform_SetWindowPos(const ImGuiViewport *viewport, const f32v2 &pos)
{
    Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->IgnoreWindowPosEventFrame = u32(ImGui::GetFrameCount());
    Graph::Window_SetPosition(vdata->Window->GetHandle(), i32v2{pos});
}

static u32v2 platform_GetWindowSize(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    return Graph::Window_GetScreenSize(vdata->Window->GetHandle());
    // return vdata->Window->GetSize();
}

static void platform_SetWindowSize(const ImGuiViewport *viewport, const f32v2 &size)
{
    Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->IgnoreWindowSizeEventFrame = u32(ImGui::GetFrameCount());
    Graph::Window_SetScreenSize(vdata->Window->GetHandle(), size);
}

static ImVec2 platform_GetWindowFramebufferScale(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    ImVec2 fbscale;
    platform_GetWindowSizeAndFramebufferScale(vdata->Window->GetHandle(), nullptr, &fbscale);
    return fbscale;
}

static void platform_SetWindowTitle(const ImGuiViewport *viewport, const char *title)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->Window->SetTitle(title);
}

static void platform_ShowWindow(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->Window->Show();
}

static void platform_SetWindowFocus(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->Window->Focus();
}

static bool platform_GetWindowFocus(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    return (vdata->Window->GetFlags() & WindowFlag_NotFocused) == 0;
}

static bool platform_GetWindowIconified(const ImGuiViewport *viewport)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    return (vdata->Window->GetFlags() & WindowFlag_Iconified) != 0;
}

static void platform_SetWindowOpacity(const ImGuiViewport *viewport, const f32 opacity)
{
    const Platform_ViewportData *vdata = platform_GetViewportData(viewport);
    vdata->Window->SetOpacity(opacity);
}

static void platform_InitMultiViewportSupport()
{
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    pio.Platform_CreateWindow = [](ImGuiViewport *v) { platform_CreateWindow(v); };
    pio.Platform_DestroyWindow = platform_DestroyWindow;
    pio.Platform_ShowWindow = [](ImGuiViewport *v) { platform_ShowWindow(v); };
    pio.Platform_SetWindowPos = [](ImGuiViewport *v, const ImVec2 pos) {
        platform_SetWindowPos(v, i32v2{pos.x, pos.y});
    };
    pio.Platform_GetWindowPos = [](ImGuiViewport *v) {
        const i32v2 pos = platform_GetWindowPos(v);
        return ImVec2{f32(pos[0]), f32(pos[1])};
    };
    pio.Platform_SetWindowSize = [](ImGuiViewport *v, const ImVec2 size) {
        platform_SetWindowSize(v, u32v2{size.x, size.y});
    };
    pio.Platform_GetWindowSize = [](ImGuiViewport *v) {
        const u32v2 size = platform_GetWindowSize(v);
        return ImVec2{f32(size[0]), f32(size[1])};
    };
    pio.Platform_GetWindowFramebufferScale = [](ImGuiViewport *v) { return platform_GetWindowFramebufferScale(v); };
    pio.Platform_SetWindowFocus = [](ImGuiViewport *v) { platform_SetWindowFocus(v); };
    pio.Platform_GetWindowFocus = [](ImGuiViewport *v) { return platform_GetWindowFocus(v); };
    pio.Platform_GetWindowMinimized = [](ImGuiViewport *v) { return platform_GetWindowIconified(v); };
    pio.Platform_SetWindowTitle = [](ImGuiViewport *v, const char *title) { platform_SetWindowTitle(v, title); };
    pio.Platform_SetWindowAlpha = [](ImGuiViewport *v, const f32 opacity) { platform_SetWindowOpacity(v, opacity); };

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    const Platform_ContextData *pdata = platform_GetContextData();
    Platform_ViewportData *vdata = platform_CreateViewportData(pdata->Window);
    mviewport->PlatformUserData = vdata;
}

static void platform_ShutdownMultiViewportSupport()
{
    const ImGuiContext *ctx = GImGui;
    const u32 size = u32(ctx->Viewports.Size);
    for (u32 i = 1; i < size; ++i)
    {
        ImGuiViewportP *viewport = ctx->Viewports[i];
        platform_DestroyWindow(viewport);
        viewport->PlatformWindowCreated = false;
        viewport->ClearRequestFlags();
    }

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    Platform_ViewportData *vdata = platform_GetViewportData(mviewport);
    platform_DestroyViewportData(vdata);

    mviewport->PlatformUserData = nullptr;
}

static void platform_Init(Window *window)
{
    ImGuiIO &io = ImGui::GetIO();
    TKIT_ASSERT(!io.BackendPlatformUserData, "[ONYX][IMGUI] Platform backend is already initialized");

    TKit::TierAllocator *tier = TKit::GetTier();
    Platform_ContextData *pdata = tier->Create<Platform_ContextData>();

    pdata->Context = ImGui::GetCurrentContext();
    pdata->Window = window;
    for (Graph::Window &owner : pdata->KeyOwnerWindows)
        owner = Graph::NullHandle;

    platform_MapAdd(window->GetHandle(), pdata);

    io.BackendPlatformUserData = scast<void *>(pdata);
    io.BackendPlatformName = "imgui_impl_onyx";
    io.BackendFlags |=
        ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_PlatformHasViewports;

    // NOTE: ImGuiBackendFlags_HasMouseHoveredViewport is not set, as Graph does not expose GLFW_HOVERED nor
    // GLFW_MOUSE_PASSTHROUGH. ImGui falls back to its own heuristic to find the viewport under the mouse

    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    pio.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *text) { Graph::SetClipboard(text); };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext *) { return Graph::GetClipboard(); };

    platform_InstallCallbacks(pdata, window->GetHandle());
    platform_UpdateMonitors();

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    mviewport->PlatformHandle = window;

    platform_InitMultiViewportSupport();
}

static void platform_UpdateMouseData(Platform_ContextData *pdata, ImGuiIO &io)
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

    const ImVec2 prevmpos = io.MousePos;
    const u32 size = u32(pio.Viewports.Size);
    for (u32 n = 0; n < size; n++)
    {
        ImGuiViewport *viewport = pio.Viewports[n];
        Window *window = scast<Window *>(viewport->PlatformHandle);

        const bool windowFocused = (window->GetFlags() & WindowFlag_NotFocused) == 0;
        if (windowFocused)
        {
            // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when
            // io.ConfigNavMoveSetMousePos is enabled by user) When multi-viewports are enabled, all Dear ImGui
            // positions are same as OS positions.

            if (io.WantSetMousePos)
                Graph::Window_SetCursorScreenPosition(
                    window->GetHandle(), f32v2{prevmpos.x - viewport->Pos.x, prevmpos.y - viewport->Pos.y});

            // (Optional) Fallback to provide mouse position when focused (platform_CursorPosCallback already
            // provides this when hovered or captured)
            if (pdata->MouseWindow == Graph::NullHandle)
            {
                f32v2 mpos = window->GetMousePosition();
                if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                {
                    // Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the
                    // mouse is on the upper-left corner of the app window) Multi-viewport mode: mouse position in OS
                    // absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary
                    // monitor)
                    const i32v2 wpos = Graph::Window_GetPosition(window->GetHandle());
                    mpos[0] += f32(wpos[0]);
                    mpos[1] += f32(wpos[1]);
                }
                pdata->LastValidMousePos = mpos;
                io.AddMousePosEvent(mpos[0], mpos[1]);
            }
        }

        // (Optional) When using multiple viewports: call io.AddMouseViewportEvent() with the viewport the OS mouse
        // cursor is hovering. If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, Dear imGui will
        // ignore this field and infer the information using its flawed heuristic. Graph does not expose
        // GLFW_HOVERED nor GLFW_MOUSE_PASSTHROUGH, so the flag is not set and this is left to ImGui
    }
}

static void platform_UpdateMouseCursor(Platform_ContextData *pdata, ImGuiIO &io)
{
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) ||
        Graph::Window_GetCursorMode(pdata->Window->GetHandle()) == Graph::CursorMode_Disabled)
        return;

    const ImGuiMouseCursor imcursor = ImGui::GetMouseCursor();
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    for (i32 n = 0; n < pio.Viewports.Size; n++)
    {
        const Graph::Window window = scast<Window *>(pio.Viewports[n]->PlatformHandle)->GetHandle();
        Platform_ViewportData *vdata = platform_GetViewportData(pio.Viewports[n]);
        if (imcursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
        {
            if (vdata->LastMouseCursor != Graph::MouseCursor_Count)
            {
                // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
                Graph::Window_SetCursorMode(window, Graph::CursorMode_Hidden);
                vdata->LastMouseCursor = Graph::MouseCursor_Count;
            }
        }
        else
        {
            // Show OS mouse cursor
            // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works
            // here.
            const Graph::MouseCursor cursor = s_PlatformData.MouseCursors[imcursor];
            if (vdata->LastMouseCursor != cursor)
            {
                Graph::Window_SetMouseCursor(window, cursor);
                vdata->LastMouseCursor = cursor;
            }
            Graph::Window_SetCursorMode(window, Graph::CursorMode_Normal);
        }
    }
}

static void platform_NewFrame()
{
    Platform_ContextData *pdata = platform_GetContextData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] Platform data has not been initialized");

    ImGuiIO &io = ImGui::GetIO();
    platform_GetWindowSizeAndFramebufferScale(pdata->Window->GetHandle(), &io.DisplaySize, &io.DisplayFramebufferScale);
    platform_UpdateMonitors();

    f64 time = pdata->Clock.GetElapsed().As<TKit::Timespan::Seconds, f64>();
    if (time < pdata->Time)
        time = pdata->Time + 1.f / 60.f;

    io.DeltaTime = f32(time - pdata->Time);
    pdata->Time = time;

    pdata->MouseIgnoreButtonUp = false;
    platform_UpdateMouseData(pdata, io);
    platform_UpdateMouseCursor(pdata, io);

    TKIT_ASSERT((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0,
                "[ONYX][IMGUI] Gamepad controls are not supported");
}

static void platform_RestoreCallbacks(const Platform_ContextData *pdata)
{
    const Graph::Window window = pdata->Window->GetHandle();
    Graph::Window_Callback_Focus(window, pdata->UserCbkWindowFocus);
    Graph::Window_Callback_CursorEnter(window, pdata->UserCbkCursorEnter);
    Graph::Window_Callback_CursorPos(window, pdata->UserCbkCursorPos);
    Graph::Window_Callback_MouseButton(window, pdata->UserCbkMousebutton);
    Graph::Window_Callback_Scroll(window, pdata->UserCbkScroll);
    Graph::Window_Callback_Key(window, pdata->UserCbkKey);
    Graph::Window_Callback_Char(window, pdata->UserCbkChar);
}

static void platform_Shutdown()
{
    const Platform_ContextData *pdata = platform_GetContextData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] No platform data to shutdown");

    platform_ShutdownMultiViewportSupport();
    platform_RestoreCallbacks(pdata);

    ImGuiIO &io = ImGui::GetIO();
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &=
        ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad |
          ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport);

    pio.ClearPlatformHandlers();
    ImGuiViewport *mviewport = ImGui::GetMainViewport();

    mviewport->PlatformHandle = nullptr;

    platform_MapRemove(pdata->Window->GetHandle());
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(pdata);
}

static constexpr u32 s_VertexShader[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000002e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001,
    0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x000a000f, 0x00000000,
    0x00000004, 0x6e69616d, 0x00000000, 0x0000000b, 0x0000000f, 0x00000015, 0x0000001b, 0x0000001c, 0x00030003,
    0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00030005, 0x00000009, 0x00000000,
    0x00050006, 0x00000009, 0x00000000, 0x6f6c6f43, 0x00000072, 0x00040006, 0x00000009, 0x00000001, 0x00005655,
    0x00030005, 0x0000000b, 0x0074754f, 0x00040005, 0x0000000f, 0x6c6f4361, 0x0000726f, 0x00030005, 0x00000015,
    0x00565561, 0x00060005, 0x00000019, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000019,
    0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000001b, 0x00000000, 0x00040005, 0x0000001c,
    0x736f5061, 0x00000000, 0x00060005, 0x0000001e, 0x73755075, 0x6e6f4368, 0x6e617473, 0x00000074, 0x00050006,
    0x0000001e, 0x00000000, 0x61635375, 0x0000656c, 0x00060006, 0x0000001e, 0x00000001, 0x61725475, 0x616c736e,
    0x00006574, 0x00030005, 0x00000020, 0x00006370, 0x00040047, 0x0000000b, 0x0000001e, 0x00000000, 0x00040047,
    0x0000000f, 0x0000001e, 0x00000002, 0x00040047, 0x00000015, 0x0000001e, 0x00000001, 0x00050048, 0x00000019,
    0x00000000, 0x0000000b, 0x00000000, 0x00030047, 0x00000019, 0x00000002, 0x00040047, 0x0000001c, 0x0000001e,
    0x00000000, 0x00050048, 0x0000001e, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000001e, 0x00000001,
    0x00000023, 0x00000008, 0x00030047, 0x0000001e, 0x00000002, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
    0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040017,
    0x00000008, 0x00000006, 0x00000002, 0x0004001e, 0x00000009, 0x00000007, 0x00000008, 0x00040020, 0x0000000a,
    0x00000003, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000003, 0x00040015, 0x0000000c, 0x00000020,
    0x00000001, 0x0004002b, 0x0000000c, 0x0000000d, 0x00000000, 0x00040020, 0x0000000e, 0x00000001, 0x00000007,
    0x0004003b, 0x0000000e, 0x0000000f, 0x00000001, 0x00040020, 0x00000011, 0x00000003, 0x00000007, 0x0004002b,
    0x0000000c, 0x00000013, 0x00000001, 0x00040020, 0x00000014, 0x00000001, 0x00000008, 0x0004003b, 0x00000014,
    0x00000015, 0x00000001, 0x00040020, 0x00000017, 0x00000003, 0x00000008, 0x0003001e, 0x00000019, 0x00000007,
    0x00040020, 0x0000001a, 0x00000003, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000003, 0x0004003b,
    0x00000014, 0x0000001c, 0x00000001, 0x0004001e, 0x0000001e, 0x00000008, 0x00000008, 0x00040020, 0x0000001f,
    0x00000009, 0x0000001e, 0x0004003b, 0x0000001f, 0x00000020, 0x00000009, 0x00040020, 0x00000021, 0x00000009,
    0x00000008, 0x0004002b, 0x00000006, 0x00000028, 0x00000000, 0x0004002b, 0x00000006, 0x00000029, 0x3f800000,
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
    0x00000010, 0x0000000f, 0x00050041, 0x00000011, 0x00000012, 0x0000000b, 0x0000000d, 0x0003003e, 0x00000012,
    0x00000010, 0x0004003d, 0x00000008, 0x00000016, 0x00000015, 0x00050041, 0x00000017, 0x00000018, 0x0000000b,
    0x00000013, 0x0003003e, 0x00000018, 0x00000016, 0x0004003d, 0x00000008, 0x0000001d, 0x0000001c, 0x00050041,
    0x00000021, 0x00000022, 0x00000020, 0x0000000d, 0x0004003d, 0x00000008, 0x00000023, 0x00000022, 0x00050085,
    0x00000008, 0x00000024, 0x0000001d, 0x00000023, 0x00050041, 0x00000021, 0x00000025, 0x00000020, 0x00000013,
    0x0004003d, 0x00000008, 0x00000026, 0x00000025, 0x00050081, 0x00000008, 0x00000027, 0x00000024, 0x00000026,
    0x00050051, 0x00000006, 0x0000002a, 0x00000027, 0x00000000, 0x00050051, 0x00000006, 0x0000002b, 0x00000027,
    0x00000001, 0x00070050, 0x00000007, 0x0000002c, 0x0000002a, 0x0000002b, 0x00000028, 0x00000029, 0x00050041,
    0x00000011, 0x0000002d, 0x0000001b, 0x0000000d, 0x0003003e, 0x0000002d, 0x0000002c, 0x000100fd, 0x00010038};

static constexpr u32 s_FragmentShader[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000001e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001,
    0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0007000f, 0x00000004,
    0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00030010, 0x00000004, 0x00000007, 0x00030003,
    0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005, 0x00000009, 0x6c6f4366,
    0x0000726f, 0x00030005, 0x0000000b, 0x00000000, 0x00050006, 0x0000000b, 0x00000000, 0x6f6c6f43, 0x00000072,
    0x00040006, 0x0000000b, 0x00000001, 0x00005655, 0x00030005, 0x0000000d, 0x00006e49, 0x00050005, 0x00000016,
    0x78655473, 0x65727574, 0x00000000, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
    0x0000001e, 0x00000000, 0x00040047, 0x00000016, 0x00000022, 0x00000000, 0x00040047, 0x00000016, 0x00000021,
    0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b,
    0x00000008, 0x00000009, 0x00000003, 0x00040017, 0x0000000a, 0x00000006, 0x00000002, 0x0004001e, 0x0000000b,
    0x00000007, 0x0000000a, 0x00040020, 0x0000000c, 0x00000001, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d,
    0x00000001, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e, 0x0000000f, 0x00000000,
    0x00040020, 0x00000010, 0x00000001, 0x00000007, 0x00090019, 0x00000013, 0x00000006, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x00000014, 0x00000013, 0x00040020, 0x00000015,
    0x00000000, 0x00000014, 0x0004003b, 0x00000015, 0x00000016, 0x00000000, 0x0004002b, 0x0000000e, 0x00000018,
    0x00000001, 0x00040020, 0x00000019, 0x00000001, 0x0000000a, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000010, 0x00000011, 0x0000000d, 0x0000000f, 0x0004003d,
    0x00000007, 0x00000012, 0x00000011, 0x0004003d, 0x00000014, 0x00000017, 0x00000016, 0x00050041, 0x00000019,
    0x0000001a, 0x0000000d, 0x00000018, 0x0004003d, 0x0000000a, 0x0000001b, 0x0000001a, 0x00050057, 0x00000007,
    0x0000001c, 0x00000017, 0x0000001b, 0x00050085, 0x00000007, 0x0000001d, 0x00000012, 0x0000001c, 0x0003003e,
    0x00000009, 0x0000001d, 0x000100fd, 0x00010038};

struct Renderer_GlobalData
{
    Graph::Sampler Sampler = Graph::NullHandle;
    Graph::DescriptorSetLayout DescriptorSetLayout = Graph::NullHandle;
    Graph::PipelineLayout PipelineLayout = Graph::NullHandle;
    Graph::Pipeline Pipeline = Graph::NullHandle;

    Graph::Shader VertexShader = Graph::NullHandle;
    Graph::Shader FragmentShader = Graph::NullHandle;
};
static TKit::Storage<Renderer_GlobalData> s_RendererData{};

struct Renderer_Buffers
{
    Graph::Buffer VertexBuffer = Graph::NullHandle;
    Graph::Buffer IndexBuffer = Graph::NullHandle;
};

struct Renderer_ViewportData
{
    Window *Window;
    TKit::TierArray<Renderer_Buffers> Buffers{};
    u32 Index = 0;
};

struct Renderer_Texture
{
    Graph::Image Image = Graph::NullHandle;
    Graph::DescriptorSet Set = Graph::NullHandle;
};

static Renderer_ViewportData *renderer_CreateViewportData(Window *window)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Renderer_ViewportData *vdata = tier->Create<Renderer_ViewportData>();
    vdata->Window = window;

    const u32 imageCount = window->GetSwapchainImageCount();
    const Specs &specs = GetSpecs();
    for (u32 i = 0; i < imageCount; ++i)
    {
        Renderer_Buffers &buffers = vdata->Buffers.Append();
        buffers.VertexBuffer = Graph::Buffer_Create<ImDrawVert>(specs.MaxImGuiVertices, Graph::BufferFlag_HostMapped |
                                                                                            Graph::BufferFlag_Vertex);
        buffers.IndexBuffer = Graph::Buffer_Create<ImDrawIdx>(specs.MaxImGuiIndices,
                                                              Graph::BufferFlag_HostMapped | Graph::BufferFlag_Index);

        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString vbuffer = TKit::StackString::Format("onyx-imgui-vbuffer-image-index-{}", i);
            const TKit::StackString ibuffer = TKit::StackString::Format("onyx-imgui-ibuffer-image-index-{}", i);
            Graph::Buffer_SetName(buffers.VertexBuffer, vbuffer.CString());
            Graph::Buffer_SetName(buffers.IndexBuffer, ibuffer.CString());
        }
    }
    return vdata;
}

static void renderer_DestroyViewportData(Renderer_ViewportData *data)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Graph::DeviceWaitIdle();
    for (Renderer_Buffers &buffers : data->Buffers)
    {
        Graph::Buffer_Destroy(buffers.VertexBuffer);
        Graph::Buffer_Destroy(buffers.IndexBuffer);
    }
    tier->Destroy(data);
}

static Renderer_ViewportData *renderer_GetViewportData(const ImGuiViewport *viewport)
{
    return scast<Renderer_ViewportData *>(viewport->RendererUserData);
}
static Renderer_Texture *renderer_GetTexture(const ImTextureData *tex)
{
    return scast<Renderer_Texture *>(tex->BackendUserData);
}

static void renderer_CreateShaders()
{
    TKIT_ASSERT(s_RendererData->VertexShader == Graph::NullHandle,
                "[ONYX][IMGUI] Vertex shader is already initialized");
    TKIT_ASSERT(s_RendererData->FragmentShader == Graph::NullHandle,
                "[ONYX][IMGUI] Fragment shader is already initialized");

    s_RendererData->VertexShader = Graph::Shader_Create(s_VertexShader, sizeof(s_VertexShader));
    s_RendererData->FragmentShader = Graph::Shader_Create(s_FragmentShader, sizeof(s_FragmentShader));
}

static Graph::Pipeline renderer_CreatePipeline()
{
    const Graph::ShaderStageInfo stages[] = {
        {.Shader = s_RendererData->VertexShader, .Stage = Graph::ShaderStageFlag_Vertex},
        {.Shader = s_RendererData->FragmentShader, .Stage = Graph::ShaderStageFlag_Fragment}};

    const Graph::VertexBinding binding{.Stride = sizeof(ImDrawVert)};

    // Locations are assigned in order: 0 -> position, 1 -> uv, 2 -> color
    const Graph::VertexAttribute attributes[] = {
        {.Offset = offsetof(ImDrawVert, pos), .Format = Graph::Format_R32G32_SFLOAT},
        {.Offset = offsetof(ImDrawVert, uv), .Format = Graph::Format_R32G32_SFLOAT},
        {.Offset = offsetof(ImDrawVert, col), .Format = Graph::Format_R8G8B8A8_UNORM}};

    // The default blend factors are the ones ImGui expects
    const Graph::ColorAttachment attachment{.Format = GetAttachmentFormat(AttachmentKind_LinearColor),
                                            .BlendEnabled = true};

    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.VertexBindings = binding;
    specs.VertexAttributes = attributes;
    specs.ColorAttachments = attachment;
    specs.DepthStencil.DepthTestEnabled = false;
    specs.DepthStencil.DepthWriteEnabled = false;

    return Graph::Pipeline_CreateGraphics(s_RendererData->PipelineLayout, specs);
}

static Graph::DescriptorSet renderer_AddTexture(const Graph::Image image)
{
    const Graph::DescriptorSet set = Graph::DescriptorSet_Create(s_RendererData->DescriptorSetLayout);

    const Graph::DescriptorImageInfo info = Graph::DescriptorImageInfo::Create(
        Graph::Image_GetView(image), Graph::ImageLayout_ShaderReadOnly, s_RendererData->Sampler);
    Graph::DescriptorSet_Write(set, 0, info);
    return set;
}

static void renderer_DestroyTexture(ImTextureData *tex)
{
    Renderer_Texture *bckTex = renderer_GetTexture(tex);
    TKIT_ASSERT(bckTex, "[ONYX][IMGUI] Texture has no backend counterpart");
    TKIT_ASSERT(bckTex->Set == scast<Graph::DescriptorSet>(tex->TexID),
                "[ONYX][IMGUI] Backend texture descriptor set mismatch");

    Graph::DescriptorSet_Destroy(bckTex->Set);
    Graph::Image_Destroy(bckTex->Image);

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(bckTex);

    tex->SetTexID(ImTextureID_Invalid);
    tex->BackendUserData = nullptr;
    tex->SetStatus(ImTextureStatus_Destroyed);
}

static void renderer_UpdateTexture(ImTextureData *tex, const u32 imageCount)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        // IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        TKIT_ASSERT(tex->TexID == ImTextureID_Invalid && !tex->BackendUserData);
        TKIT_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        TKit::TierAllocator *tier = TKit::GetTier();
        Renderer_Texture *bckTex = tier->Create<Renderer_Texture>();

        const Graph::Format format = Graph::Format_R8G8B8A8_UNORM;
        const Graph::ImageViewSpecs view{};

        Graph::ImageSpecs specs{};
        specs.ImageViews = view;
        specs.Formats = format;
        bckTex->Image =
            Graph::Image_Create(u32v2{u32(tex->Width), u32(tex->Height)}, specs,
                                Graph::ImageFlag_Sampled | Graph::ImageFlag_Destination | Graph::ImageFlag_Color);

        bckTex->Set = renderer_AddTexture(bckTex->Image);

        // Store identifiers
        tex->SetTexID(ImTextureID(bckTex->Set));
        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString tname = TKit::StackString::Format("onyx-imgui-texture-id-{:#x}", tex->GetTexID());
            const TKit::StackString sname =
                TKit::StackString::Format("onyx-imgui-tex-descriptor-id-{:#x}", tex->GetTexID());
            Graph::Image_SetName(bckTex->Image, tname.CString());
            Graph::DescriptorSet_SetName(bckTex->Set, sname.CString());
        }
        tex->BackendUserData = bckTex;
        TKIT_LOG_DEBUG("[ONYX][IMGUI] Created new texture with id '{:#x}'", tex->GetTexID());
    }

    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
    {
        TKIT_LOG_DEBUG("[ONYX][IMGUI] Updating texture with id '{:#x}'", tex->GetTexID());
        Renderer_Texture *bckTex = renderer_GetTexture(tex);
        TKIT_ASSERT(Graph::Format_GetBytesPerPixel(Graph::Format_R8G8B8A8_UNORM) == usz(tex->BytesPerPixel),
                    "[ONYX][IMGUI] Bytes per pixel mismatch between Graph::Format and ImGui texture");

        // Update full texture or selected blocks. We only ever write to textures regions which have never been used
        // before! This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual
        // regions. We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.

        const bool wantCreate = tex->Status == ImTextureStatus_WantCreate;
        const u32 xupload = wantCreate ? 0 : tex->UpdateRect.x;
        const u32 yupload = wantCreate ? 0 : tex->UpdateRect.y;
        const u32 wupload = wantCreate ? tex->Width : tex->UpdateRect.w;
        const u32 hupload = wantCreate ? tex->Height : tex->UpdateRect.h;

        const usz wsize = wupload * tex->BytesPerPixel;
        const usz size = hupload * wsize;

        const Graph::Buffer uploadBuffer =
            Graph::Buffer_Create(size, Graph::BufferFlag_Staging | Graph::BufferFlag_HostMapped);
        if (Graph::IsValidationEnabled())
            Graph::Buffer_SetName(uploadBuffer, "onyx-imgui-upload-buffer");

        std::byte *mem = scast<std::byte *>(Graph::Buffer_GetData(uploadBuffer));
        for (u32 y = 0; y < hupload; ++y)
            TKit::ForwardCopy(mem + wsize * y, tex->GetPixelsAt(i32(xupload), i32(yupload + y)), wsize);

        Graph::Buffer_Flush(uploadBuffer);

        Graph::CommandPool_ImmediateSubmission(GetTransientGraphicsCommandPool(), [&](const Graph::CommandBuffer cmd) {
            const Graph::BufferMemoryBarrierInfo uploadBarrier{.Offset = 0,
                                                               .Size = size,
                                                               .Buffer = uploadBuffer,
                                                               .SrcStages = Graph::PipelineStageFlag_Host,
                                                               .SrcAccess = Graph::AccessFlag_HostWrite,
                                                               .DstStages = Graph::PipelineStageFlag_Transfer,
                                                               .DstAccess = Graph::AccessFlag_TransferRead};

            const Graph::ImageMemoryBarrierInfo imgBarrier{.Image = bckTex->Image,
                                                           .NewLayout = Graph::ImageLayout_TransferDst,
                                                           .SrcStages = Graph::PipelineStageFlag_FragmentShader,
                                                           .DstStages = Graph::PipelineStageFlag_Transfer,
                                                           .DstAccess = Graph::AccessFlag_TransferWrite};

            Graph::Command_PipelineBarrier(cmd, {.BufferBarriers = uploadBarrier, .ImageBarriers = imgBarrier});

            const Graph::BufferImageCopy copy{.Layers = {.LayerCount = 1},
                                              .ImageOffset = {xupload, yupload, 0},
                                              .ImageExtent = {wupload, hupload, 1}};
            Graph::Command_CopyBufferToImage(cmd, uploadBuffer, bckTex->Image, copy);

            const Graph::ImageMemoryBarrierInfo readBarrier{.Image = bckTex->Image,
                                                            .NewLayout = Graph::ImageLayout_ShaderReadOnly,
                                                            .SrcStages = Graph::PipelineStageFlag_Transfer,
                                                            .SrcAccess = Graph::AccessFlag_TransferWrite,
                                                            .DstStages = Graph::PipelineStageFlag_FragmentShader,
                                                            .DstAccess = Graph::AccessFlag_ShaderRead};
            Graph::Command_ImageBarrier(cmd, readBarrier);
        });

        Graph::Buffer_Destroy(uploadBuffer);

        tex->SetStatus(ImTextureStatus_OK);
    }

    if (tex->Status == ImTextureStatus_WantDestroy && u32(tex->UnusedFrames) >= imageCount)
        renderer_DestroyTexture(tex);
}

static void renderer_Render(const ImDrawData *ddata, const Graph::CommandBuffer cmd)
{
    const f32v2 fb =
        f32v2{ddata->DisplaySize.x * ddata->FramebufferScale.x, ddata->DisplaySize.y * ddata->FramebufferScale.y};

    if (fb[0] <= 0.f || fb[1] <= 0.f)
        return;

    Renderer_ViewportData *vdata = renderer_GetViewportData(ddata->OwnerViewport);
    TKIT_ASSERT(vdata, "[ONYX][IMGUI] Renderer viewport data is null");
    TKIT_ASSERT(
        !vdata->Buffers.IsEmpty(),
        "[ONYX][IMGUI] Renderer viewport data has no buffers. They should have been created prior to this call");

    if (ddata->Textures)
        for (ImTextureData *tex : *ddata->Textures)
            if (tex->Status != ImTextureStatus_OK)
                renderer_UpdateTexture(tex, vdata->Buffers.GetSize());

    Graph::Command_BindPipeline(cmd, s_RendererData->Pipeline);

    vdata->Index = (vdata->Index + 1) % vdata->Buffers.GetSize();

    Renderer_Buffers &buffers = vdata->Buffers[vdata->Index];
    if (ddata->TotalVtxCount > 0)
    {
        TKIT_ASSERT(u32(ddata->TotalVtxCount) <= GetSpecs().MaxImGuiVertices,
                    "[ONYX][IMGUI] Maximum vertices exceeded ({} > {})! Increase the specs .MaxImGuiVertices field",
                    u32(ddata->TotalVtxCount), GetSpecs().MaxImGuiVertices);
        TKIT_ASSERT(u32(ddata->TotalIdxCount) <= GetSpecs().MaxImGuiIndices,
                    "[ONYX][IMGUI] Maximum indices exceeded ({} > {})! Increase the specs .MaxImGuiIndices field",
                    u32(ddata->TotalIdxCount), GetSpecs().MaxImGuiIndices);

        usz voffset = 0;
        usz ioffset = 0;
        for (const ImDrawList *dlist : ddata->CmdLists)
        {
            const usz lvsize = dlist->VtxBuffer.Size * sizeof(ImDrawVert);
            const usz lisize = dlist->IdxBuffer.Size * sizeof(ImDrawIdx);

            Graph::Buffer_Write(buffers.VertexBuffer, dlist->VtxBuffer.Data,
                                {.Size = lvsize, .SrcOffset = 0, .DstOffset = voffset});
            Graph::Buffer_Write(buffers.IndexBuffer, dlist->IdxBuffer.Data,
                                {.Size = lisize, .SrcOffset = 0, .DstOffset = ioffset});

            voffset += lvsize;
            ioffset += lisize;
        }

        Graph::Buffer_Flush(buffers.VertexBuffer);
        Graph::Buffer_Flush(buffers.IndexBuffer);

        Graph::Command_BindVertexBuffers(cmd, buffers.VertexBuffer);
        Graph::Command_BindIndexBuffer(cmd, buffers.IndexBuffer,
                                       sizeof(ImDrawIdx) == 2 ? Graph::IndexType_Unsigned16
                                                              : Graph::IndexType_Unsigned32);
    }

    Graph::Command_SetViewport(cmd, f32v2{0.f}, fb);

    struct PushConstant
    {
        f32v2 sc;
        f32v2 t;
    };

    PushConstant pc;
    pc.sc = f32v2{2.f / ddata->DisplaySize.x, 2.f / ddata->DisplaySize.y};
    pc.t = f32v2{-1.f - ddata->DisplayPos.x * pc.sc[0], -1.f - ddata->DisplayPos.y * pc.sc[1]};

    Graph::Command_PushConstants(cmd, s_RendererData->PipelineLayout, Graph::ShaderStageFlag_Vertex, &pc);

    const f32v2 clipOff = {ddata->DisplayPos.x, ddata->DisplayPos.y};
    const f32v2 clipScale = {ddata->FramebufferScale.x, ddata->FramebufferScale.y};

    Graph::DescriptorSet lastSet = Graph::NullHandle;

    u32 voffset = 0;
    u32 ioffset = 0;
    for (const ImDrawList *dlist : ddata->CmdLists)
    {
        for (u32 i = 0; i < u32(dlist->CmdBuffer.Size); ++i)
        {
            const ImDrawCmd &icmd = dlist->CmdBuffer[i];

            f32v2 clipMin{(icmd.ClipRect.x - clipOff[0]) * clipScale[0], (icmd.ClipRect.y - clipOff[1]) * clipScale[1]};
            f32v2 clipMax{(icmd.ClipRect.z - clipOff[0]) * clipScale[0], (icmd.ClipRect.w - clipOff[1]) * clipScale[1]};

            clipMin = Math::Max(clipMin, f32v2{0.f});
            clipMax = Math::Min(clipMax, fb);
            if (clipMax[0] <= clipMin[0] || clipMax[1] <= clipMin[1])
                continue;

            Graph::Command_SetScissor(cmd, i32v2{i32(clipMin[0]), i32(clipMin[1])},
                                      u32v2{u32(clipMax[0] - clipMin[0]), u32(clipMax[1] - clipMin[1])});

            const Graph::DescriptorSet set = scast<Graph::DescriptorSet>(icmd.GetTexID());
            if (set != lastSet)
                Graph::Command_BindDescriptorSets(cmd, s_RendererData->PipelineLayout, Graph::BindPoint_Graphics, set);
            lastSet = set;

            // NOTE: Graph::Command_DrawIndexed takes firstInstance before vertexOffset (the reverse of Vulkan)
            Graph::Command_DrawIndexed(cmd, icmd.ElemCount, 1, icmd.IdxOffset + ioffset, 0, icmd.VtxOffset + voffset);
        }
        voffset += u32(dlist->VtxBuffer.Size);
        ioffset += u32(dlist->IdxBuffer.Size);
    }

    Graph::Command_SetScissor(cmd, i32v2{0}, u32v2{u32(fb[0]), u32(fb[1])});
}

static void renderer_CreateDeviceObjects()
{
    // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or
    // 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.

    Graph::SamplerSpecs sspecs{};
    sspecs.Wraps = Graph::Wrap_Repeat;
    sspecs.LodRange = f32v2{-1000.f, 1000.f};
    s_RendererData->Sampler = Graph::Sampler_Create(sspecs);

    const Graph::DescriptorBinding dbinding =
        Graph::DescriptorBinding::Create(0, Graph::Descriptor_CombinedImageSampler, Graph::ShaderStageFlag_Fragment);
    s_RendererData->DescriptorSetLayout = Graph::DescriptorSetLayout_Create(dbinding);

    const Graph::PushConstantRange range =
        Graph::PushConstantRange::Create(Graph::ShaderStageFlag_Vertex, 4 * sizeof(f32));
    s_RendererData->PipelineLayout = Graph::PipelineLayout_Create(
        {.DescriptorSetLayouts = s_RendererData->DescriptorSetLayout, .PushRanges = range});

    {
        renderer_CreateShaders();
        s_RendererData->Pipeline = renderer_CreatePipeline();
    }

    if (Graph::IsValidationEnabled())
    {
        Graph::Sampler_SetName(s_RendererData->Sampler, "onyx-imgui-sampler");
        Graph::DescriptorSetLayout_SetName(s_RendererData->DescriptorSetLayout, "onyx-imgui-descriptor-set-layout");
        Graph::PipelineLayout_SetName(s_RendererData->PipelineLayout, "onyx-imgui-pipeline-layout");
        Graph::Shader_SetName(s_RendererData->VertexShader, "onyx-imgui-vertex-shader");
        Graph::Shader_SetName(s_RendererData->FragmentShader, "onyx-imgui-fragment-shader");
        Graph::Pipeline_SetName(s_RendererData->Pipeline, "onyx-imgui-pipeline");
    }
}

static void renderer_CreateWindow(ImGuiViewport *viewport)
{
    const Platform_ViewportData *pvdata = platform_GetViewportData(viewport);
    TKIT_ASSERT(pvdata && pvdata->Window,
                "[ONYX][IMGUi] Platform viewport data must be created before renderer viewport data");
    viewport->RendererUserData = renderer_CreateViewportData(pvdata->Window);
}

static void renderer_DestroyWindow(ImGuiViewport *viewport)
{
    Renderer_ViewportData *vdata = renderer_GetViewportData(viewport);
    TKIT_LOG_WARNING_IF(!vdata, "[ONYX][IMGUI] Renderer viewport data is null when destroying platform window");

    if (vdata)
        renderer_DestroyViewportData(vdata);
    viewport->RendererUserData = nullptr;
}

static bool renderer_AcquireImage(const ImGuiViewport *viewport, const u64 timeout)
{
    const Renderer_ViewportData *vdata = renderer_GetViewportData(viewport);
    TKIT_ASSERT(vdata, "[ONYX][IMGUI] Platform viewport data is null");
    return vdata->Window->AcquireNextImage(timeout);
}

static RenderSubmitInfo renderer_RenderWindow(const ImGuiViewport *viewport, const Graph::CommandBuffer cmd)
{
    const Renderer_ViewportData *vdata = renderer_GetViewportData(viewport);
    TKIT_ASSERT(vdata, "[ONYX][IMGUI] Renderer viewport data is null");

    Window *window = vdata->Window;
    const u64 graphicsFlight = Graph::Queue_ReserveTimelineValue(Graph::Queue_Graphics);

    window->BeginRendering(cmd);
    renderer_Render(viewport->DrawData, cmd);
    window->EndRendering(cmd);

    // Graph takes care of the image available/render finished semaphores when the swap chain is passed on submission
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = cmd;
    submitInfo.Swapchain = window->GetSwapchain();
    submitInfo.InFlightValue = graphicsFlight;
    return submitInfo;
}

static void renderer_PresentWindow(const ImGuiViewport *viewport)
{
    const Renderer_ViewportData *vdata = renderer_GetViewportData(viewport);
    TKIT_ASSERT(vdata, "[ONYX][IMGUI] Renderer viewport data is null");

    return vdata->Window->Present();
}

static void renderer_InitMultiViewportSupport()
{
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    pio.Renderer_CreateWindow = [](ImGuiViewport *v) { renderer_CreateWindow(v); };
    pio.Renderer_DestroyWindow = [](ImGuiViewport *v) { renderer_DestroyWindow(v); };
    // pio.Renderer_RenderWindow = [](ImGuiViewport *v) { ONYX_CHECK_VKIT_RESULT(renderer_RenderWindow(v)); };
}

static void renderer_ShutdownMultiViewportSupport()
{
    const ImGuiContext *ctx = GImGui;
    const u32 size = u32(ctx->Viewports.Size);
    for (u32 i = 1; i < size; ++i)
    {
        ImGuiViewportP *viewport = ctx->Viewports[i];
        renderer_DestroyWindow(viewport);
        viewport->PlatformWindowCreated = false;
        viewport->ClearRequestFlags();
    }
}

static void renderer_Init(Window *window)
{
    ImGuiIO &io = ImGui::GetIO();

    io.BackendRendererName = "imgui_impl_onyx";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasViewports |
                       ImGuiBackendFlags_RendererHasVtxOffset;

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    mviewport->RendererUserData = renderer_CreateViewportData(window);

    renderer_InitMultiViewportSupport();
}

static void renderer_Shutdown()
{
    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    Renderer_ViewportData *vdata = renderer_GetViewportData(mviewport);

    renderer_ShutdownMultiViewportSupport();

    for (ImTextureData *tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
            renderer_DestroyTexture(tex);

    renderer_DestroyViewportData(vdata);
    mviewport->RendererUserData = nullptr;

    ImGuiIO &io = ImGui::GetIO();
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

    io.BackendRendererName = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures |
                         ImGuiBackendFlags_RendererHasViewports);

    pio.ClearRendererHandlers();
}

void ImGuiBackend_Create(Window *window)
{
    platform_Init(window);
    return renderer_Init(window);
}

void ImGuiBackend_DestroyCurrent()
{
    Graph::DeviceWaitIdle();
    renderer_Shutdown();
    platform_Shutdown();
    ImGui::DestroyPlatformWindows();
}

void ImGuiBackend_Initialize()
{
    TKit::FixedArray<Graph::MouseCursor, ImGuiMouseCursor_COUNT> &cursors = s_PlatformData.MouseCursors;
    // ImGui cursors Graph has no equivalent for fall back to the arrow
    for (Graph::MouseCursor &cursor : cursors)
        cursor = Graph::MouseCursor_Arrow;

    cursors[ImGuiMouseCursor_Arrow] = Graph::MouseCursor_Arrow;
    cursors[ImGuiMouseCursor_TextInput] = Graph::MouseCursor_IBeam;
    cursors[ImGuiMouseCursor_ResizeNS] = Graph::MouseCursor_NS;
    cursors[ImGuiMouseCursor_ResizeEW] = Graph::MouseCursor_EW;
    cursors[ImGuiMouseCursor_Hand] = Graph::MouseCursor_Hand;
    cursors[ImGuiMouseCursor_ResizeAll] = Graph::MouseCursor_ResizeAll;
    cursors[ImGuiMouseCursor_ResizeNESW] = Graph::MouseCursor_NESW;
    cursors[ImGuiMouseCursor_ResizeNWSE] = Graph::MouseCursor_NWSE;
    cursors[ImGuiMouseCursor_NotAllowed] = Graph::MouseCursor_NotAllowed;

#ifdef ONYX_IMGUI_WAYLAND
    s_PlatformData.IsWayland = Graph::GetPlatformBackendInfo().Platform == Graph::Platform_Wayland;
#endif
    s_RendererData.Construct();
    return renderer_CreateDeviceObjects();
}
void ImGuiBackend_Terminate()
{
    Graph::Pipeline_Destroy(s_RendererData->Pipeline);
    Graph::Sampler_Destroy(s_RendererData->Sampler);
    Graph::PipelineLayout_Destroy(s_RendererData->PipelineLayout);
    Graph::DescriptorSetLayout_Destroy(s_RendererData->DescriptorSetLayout);
    Graph::Shader_Destroy(s_RendererData->FragmentShader);
    Graph::Shader_Destroy(s_RendererData->VertexShader);
    s_RendererData.Destruct();
}

void ImGuiBackend_NewFrame()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::NewFrame");
    platform_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend_RenderData(ImDrawData *data, const Graph::CommandBuffer commandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::RenderData");
    return renderer_Render(data, commandBuffer);
}
void ImGuiBackend_UpdatePlatformWindows()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::UpdateWindows");
    ImGuiContext *ctx = GImGui;
    TKIT_ASSERT(ctx->FrameCountEnded == ctx->FrameCount,
                "[ONYX][IMGUI] ImGui::Render() must be called before updating windows");
    TKIT_ASSERT(ctx->FrameCountPlatformEnded < ctx->FrameCount);
    ctx->FrameCountPlatformEnded = ctx->FrameCount;

    if (!(ctx->ConfigFlagsCurrFrame & ImGuiConfigFlags_ViewportsEnable))
        return;

    const u32 size = u32(ctx->Viewports.size());
    for (u32 i = 1; i < size; ++i)
    {
        ImGuiViewportP *viewport = ctx->Viewports[i];
        const bool inactive = viewport->LastFrameActive + 1 < ctx->FrameCount;
        const bool invisible = viewport->Window && (!viewport->Window->Active || viewport->Window->Hidden);
        if (inactive || invisible)
        {
            if (viewport->PlatformWindowCreated)
            {
                renderer_DestroyWindow(viewport);
                platform_DestroyWindow(viewport);
                if (viewport->ID != ImGui::GetMainViewport()->ID)
                    viewport->PlatformWindowCreated = false;
            }
            viewport->ClearRequestFlags();
            viewport->PlatformHandle = nullptr;
            viewport->PlatformUserData = nullptr;
            viewport->RendererUserData = nullptr;
            continue;
        }
        if (viewport->LastFrameActive < ctx->FrameCount || viewport->Size.x <= 0 || viewport->Size.y <= 0)
            continue;

        const bool newWindow = !viewport->PlatformWindowCreated;
        if (newWindow)
        {
            platform_CreateWindow(viewport);
            renderer_CreateWindow(viewport);
            ++ctx->PlatformWindowsCreatedCount;
            viewport->LastNameHash = 0;

            // By clearing those we'll enforce a call to Platform_SetWindowPos/Size below,
            // before Platform_ShowWindow (FIXME: Is that necessary?)

            viewport->LastPlatformPos = ImVec2(FLT_MAX, FLT_MAX);
            viewport->LastPlatformSize = ImVec2(FLT_MAX, FLT_MAX);

            // We don't need to call Renderer_SetWindowSize() as it is
            // expected Renderer_CreateWindow() already did it.
            viewport->LastRendererSize = viewport->Size;
            viewport->PlatformWindowCreated = true;
        }
        if ((viewport->LastPlatformPos.x != viewport->Pos.x || viewport->LastPlatformPos.y != viewport->Pos.y) &&
            !viewport->PlatformRequestMove)
            platform_SetWindowPos(viewport, f32v2{viewport->Pos.x, viewport->Pos.y});

        if ((viewport->LastPlatformSize.x != viewport->Size.x || viewport->LastPlatformSize.y != viewport->Size.y) &&
            !viewport->PlatformRequestResize)
            platform_SetWindowSize(viewport, f32v2{viewport->Size.x, viewport->Size.y});

        viewport->LastPlatformPos = viewport->Pos;
        viewport->LastPlatformSize = viewport->Size;
        viewport->LastRendererSize = viewport->Size;

        ImGuiWindow *wtitle =
            viewport->Window->DockNodeAsHost ? viewport->Window->DockNodeAsHost->VisibleWindow : viewport->Window;
        if (wtitle)
        {
            const char *titleBegin = wtitle->Name;
            // this is horrible
            char *titleEnd = const_cast<char *>(ImGui::FindRenderedTextEnd(titleBegin));
            const ImGuiID thash = ImHashStr(titleBegin, titleEnd - titleBegin);
            if (viewport->LastNameHash != thash)
            {
                const char tend = *titleEnd;
                *titleEnd = 0;
                platform_SetWindowTitle(viewport, titleBegin);
                *titleEnd = tend;
            }
        }
        if (viewport->LastAlpha != viewport->Alpha)
            platform_SetWindowOpacity(viewport, viewport->Alpha);
        viewport->LastAlpha = viewport->Alpha;

        if (newWindow)
        {
            if (ctx->FrameCount < 3)
                viewport->Flags |= ImGuiViewportFlags_NoFocusOnAppearing;
            platform_ShowWindow(viewport);
        }

        viewport->ClearRequestFlags();
    }
}

u32 ImGuiBackend_GetPlatformWindowCount()
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    return u32(pio.Viewports.Size - 1);
}

bool ImGuiBackend_AcquirePlatformWindowImage(const u32 windowIndex, const u64 timeout)
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    ImGuiViewport *viewport = pio.Viewports[windowIndex + 1];
    if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
        return false;
    return renderer_AcquireImage(viewport, timeout);
}

RenderSubmitInfo ImGuiBackend_RenderPlatformWindow(const u32 windowIndex, const Graph::CommandBuffer cmd)
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    ImGuiViewport *viewport = pio.Viewports[windowIndex + 1];
    return renderer_RenderWindow(viewport, cmd);
}

void ImGuiBackend_PresentPlatformWindow(const u32 windowIndex)
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    ImGuiViewport *viewport = pio.Viewports[windowIndex + 1];
    return renderer_PresentWindow(viewport);
}

} // namespace Onyx
