#include "onyx/core/pch.hpp"
#include "onyx/imgui/backend.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/glfw.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/state/descriptors.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/state/descriptor_set_layout.hpp"
#include "vkit/state/pipeline_layout.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/shader.hpp"
#include "tkit/profiling/macros.hpp"

#if !defined(ONYX_IMGUI_DISABLE_X11) && (defined(TKIT_OS_LINUX) || defined(__FreeBSD__) || defined(__OpenBSD__) ||     \
                                         defined(__NetBSD__) || defined(__DragonFly__))
#    define ONYX_GLFW_X11
#endif

#if !defined(ONYX_IMGUI_DISABLE_WAYLAND) && (defined(TKIT_OS_LINUX) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
                                             defined(__NetBSD__) || defined(__DragonFly__))
#    define ONYX_GLFW_WAYLAND
#endif

namespace ImGui
{
extern ImGuiIO &GetIO(ImGuiContext *);
}

namespace Onyx::ImGuiBackend
{
struct Platform_GlobalData
{
    TKit::FixedArray<GLFWcursor *, ImGuiMouseCursor_COUNT> MouseCursors;
#ifdef ONYX_GLFW_WAYLAND
    bool IsWayland;
#endif
};
static Platform_GlobalData s_PlatformData{};

struct Platform_ContextData
{
    ImGuiContext *Context;
    Window *Window;
    GLFWcursor *LastMouseCursor;

    GLFWwindow *MouseWindow;
    GLFWwindow *KeyOwnerWindows[GLFW_KEY_LAST];

    f64 Time = 0.f;

    bool MouseIgnoreButtonUpWaitForFocusLoss;
    bool MouseIgnoreButtonUp;

    f32v2 LastValidMousePos;

    GLFWwindowfocusfun UserCbkWindowFocus;
    GLFWcursorposfun UserCbkCursorPos;
    GLFWcursorenterfun UserCbkCursorEnter;
    GLFWmousebuttonfun UserCbkMousebutton;
    GLFWscrollfun UserCbkScroll;
    GLFWkeyfun UserCbkKey;
    GLFWcharfun UserCbkChar;
};

#ifdef ONYX_GLFW_WAYLAND
static bool platform_IsWayland()
{
#    ifdef ONYX_GLFW_GETPLATFORM
    return glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#    else
    const char *version = glfwGetVersionString();
    if (!strstr(version, "Wayland")) // e.g. Ubuntu 22.04 ships with GLFW 3.3.6 compiled without Wayland
        return false;
    // #        ifdef GLFW_EXPOSE_NATIVE_X11
    //     if (glfwGetX11Display())
    //         return false;
    // #        endif
    return true;
#    endif
}
#endif

ImGuiKey platform_KeyToImGuiKey(const i32 keycode)
{
    switch (keycode)
    {
    case GLFW_KEY_TAB:
        return ImGuiKey_Tab;
    case GLFW_KEY_LEFT:
        return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT:
        return ImGuiKey_RightArrow;
    case GLFW_KEY_UP:
        return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN:
        return ImGuiKey_DownArrow;
    case GLFW_KEY_PAGE_UP:
        return ImGuiKey_PageUp;
    case GLFW_KEY_PAGE_DOWN:
        return ImGuiKey_PageDown;
    case GLFW_KEY_HOME:
        return ImGuiKey_Home;
    case GLFW_KEY_END:
        return ImGuiKey_End;
    case GLFW_KEY_INSERT:
        return ImGuiKey_Insert;
    case GLFW_KEY_DELETE:
        return ImGuiKey_Delete;
    case GLFW_KEY_BACKSPACE:
        return ImGuiKey_Backspace;
    case GLFW_KEY_SPACE:
        return ImGuiKey_Space;
    case GLFW_KEY_ENTER:
        return ImGuiKey_Enter;
    case GLFW_KEY_ESCAPE:
        return ImGuiKey_Escape;
    case GLFW_KEY_APOSTROPHE:
        return ImGuiKey_Apostrophe;
    case GLFW_KEY_COMMA:
        return ImGuiKey_Comma;
    case GLFW_KEY_MINUS:
        return ImGuiKey_Minus;
    case GLFW_KEY_PERIOD:
        return ImGuiKey_Period;
    case GLFW_KEY_SLASH:
        return ImGuiKey_Slash;
    case GLFW_KEY_SEMICOLON:
        return ImGuiKey_Semicolon;
    case GLFW_KEY_EQUAL:
        return ImGuiKey_Equal;
    case GLFW_KEY_LEFT_BRACKET:
        return ImGuiKey_LeftBracket;
    case GLFW_KEY_BACKSLASH:
        return ImGuiKey_Backslash;
    case GLFW_KEY_WORLD_1:
        return ImGuiKey_Oem102;
    case GLFW_KEY_WORLD_2:
        return ImGuiKey_Oem102;
    case GLFW_KEY_RIGHT_BRACKET:
        return ImGuiKey_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT:
        return ImGuiKey_GraveAccent;
    case GLFW_KEY_CAPS_LOCK:
        return ImGuiKey_CapsLock;
    case GLFW_KEY_SCROLL_LOCK:
        return ImGuiKey_ScrollLock;
    case GLFW_KEY_NUM_LOCK:
        return ImGuiKey_NumLock;
    case GLFW_KEY_PRINT_SCREEN:
        return ImGuiKey_PrintScreen;
    case GLFW_KEY_PAUSE:
        return ImGuiKey_Pause;
    case GLFW_KEY_KP_0:
        return ImGuiKey_Keypad0;
    case GLFW_KEY_KP_1:
        return ImGuiKey_Keypad1;
    case GLFW_KEY_KP_2:
        return ImGuiKey_Keypad2;
    case GLFW_KEY_KP_3:
        return ImGuiKey_Keypad3;
    case GLFW_KEY_KP_4:
        return ImGuiKey_Keypad4;
    case GLFW_KEY_KP_5:
        return ImGuiKey_Keypad5;
    case GLFW_KEY_KP_6:
        return ImGuiKey_Keypad6;
    case GLFW_KEY_KP_7:
        return ImGuiKey_Keypad7;
    case GLFW_KEY_KP_8:
        return ImGuiKey_Keypad8;
    case GLFW_KEY_KP_9:
        return ImGuiKey_Keypad9;
    case GLFW_KEY_KP_DECIMAL:
        return ImGuiKey_KeypadDecimal;
    case GLFW_KEY_KP_DIVIDE:
        return ImGuiKey_KeypadDivide;
    case GLFW_KEY_KP_MULTIPLY:
        return ImGuiKey_KeypadMultiply;
    case GLFW_KEY_KP_SUBTRACT:
        return ImGuiKey_KeypadSubtract;
    case GLFW_KEY_KP_ADD:
        return ImGuiKey_KeypadAdd;
    case GLFW_KEY_KP_ENTER:
        return ImGuiKey_KeypadEnter;
    case GLFW_KEY_KP_EQUAL:
        return ImGuiKey_KeypadEqual;
    case GLFW_KEY_LEFT_SHIFT:
        return ImGuiKey_LeftShift;
    case GLFW_KEY_LEFT_CONTROL:
        return ImGuiKey_LeftCtrl;
    case GLFW_KEY_LEFT_ALT:
        return ImGuiKey_LeftAlt;
    case GLFW_KEY_LEFT_SUPER:
        return ImGuiKey_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT:
        return ImGuiKey_RightShift;
    case GLFW_KEY_RIGHT_CONTROL:
        return ImGuiKey_RightCtrl;
    case GLFW_KEY_RIGHT_ALT:
        return ImGuiKey_RightAlt;
    case GLFW_KEY_RIGHT_SUPER:
        return ImGuiKey_RightSuper;
    case GLFW_KEY_MENU:
        return ImGuiKey_Menu;
    case GLFW_KEY_0:
        return ImGuiKey_0;
    case GLFW_KEY_1:
        return ImGuiKey_1;
    case GLFW_KEY_2:
        return ImGuiKey_2;
    case GLFW_KEY_3:
        return ImGuiKey_3;
    case GLFW_KEY_4:
        return ImGuiKey_4;
    case GLFW_KEY_5:
        return ImGuiKey_5;
    case GLFW_KEY_6:
        return ImGuiKey_6;
    case GLFW_KEY_7:
        return ImGuiKey_7;
    case GLFW_KEY_8:
        return ImGuiKey_8;
    case GLFW_KEY_9:
        return ImGuiKey_9;
    case GLFW_KEY_A:
        return ImGuiKey_A;
    case GLFW_KEY_B:
        return ImGuiKey_B;
    case GLFW_KEY_C:
        return ImGuiKey_C;
    case GLFW_KEY_D:
        return ImGuiKey_D;
    case GLFW_KEY_E:
        return ImGuiKey_E;
    case GLFW_KEY_F:
        return ImGuiKey_F;
    case GLFW_KEY_G:
        return ImGuiKey_G;
    case GLFW_KEY_H:
        return ImGuiKey_H;
    case GLFW_KEY_I:
        return ImGuiKey_I;
    case GLFW_KEY_J:
        return ImGuiKey_J;
    case GLFW_KEY_K:
        return ImGuiKey_K;
    case GLFW_KEY_L:
        return ImGuiKey_L;
    case GLFW_KEY_M:
        return ImGuiKey_M;
    case GLFW_KEY_N:
        return ImGuiKey_N;
    case GLFW_KEY_O:
        return ImGuiKey_O;
    case GLFW_KEY_P:
        return ImGuiKey_P;
    case GLFW_KEY_Q:
        return ImGuiKey_Q;
    case GLFW_KEY_R:
        return ImGuiKey_R;
    case GLFW_KEY_S:
        return ImGuiKey_S;
    case GLFW_KEY_T:
        return ImGuiKey_T;
    case GLFW_KEY_U:
        return ImGuiKey_U;
    case GLFW_KEY_V:
        return ImGuiKey_V;
    case GLFW_KEY_W:
        return ImGuiKey_W;
    case GLFW_KEY_X:
        return ImGuiKey_X;
    case GLFW_KEY_Y:
        return ImGuiKey_Y;
    case GLFW_KEY_Z:
        return ImGuiKey_Z;
    case GLFW_KEY_F1:
        return ImGuiKey_F1;
    case GLFW_KEY_F2:
        return ImGuiKey_F2;
    case GLFW_KEY_F3:
        return ImGuiKey_F3;
    case GLFW_KEY_F4:
        return ImGuiKey_F4;
    case GLFW_KEY_F5:
        return ImGuiKey_F5;
    case GLFW_KEY_F6:
        return ImGuiKey_F6;
    case GLFW_KEY_F7:
        return ImGuiKey_F7;
    case GLFW_KEY_F8:
        return ImGuiKey_F8;
    case GLFW_KEY_F9:
        return ImGuiKey_F9;
    case GLFW_KEY_F10:
        return ImGuiKey_F10;
    case GLFW_KEY_F11:
        return ImGuiKey_F11;
    case GLFW_KEY_F12:
        return ImGuiKey_F12;
    case GLFW_KEY_F13:
        return ImGuiKey_F13;
    case GLFW_KEY_F14:
        return ImGuiKey_F14;
    case GLFW_KEY_F15:
        return ImGuiKey_F15;
    case GLFW_KEY_F16:
        return ImGuiKey_F16;
    case GLFW_KEY_F17:
        return ImGuiKey_F17;
    case GLFW_KEY_F18:
        return ImGuiKey_F18;
    case GLFW_KEY_F19:
        return ImGuiKey_F19;
    case GLFW_KEY_F20:
        return ImGuiKey_F20;
    case GLFW_KEY_F21:
        return ImGuiKey_F21;
    case GLFW_KEY_F22:
        return ImGuiKey_F22;
    case GLFW_KEY_F23:
        return ImGuiKey_F23;
    case GLFW_KEY_F24:
        return ImGuiKey_F24;
    default:
        return ImGuiKey_None;
    }
}

struct WindowDataPair
{
    GLFWwindow *Window;
    Platform_ContextData *Data;
};
static TKit::TierArray<WindowDataPair> s_PlatformDataMap{};

static Platform_ContextData *platform_GetData(GLFWwindow *window = nullptr)
{
    if (window)
    {
        for (const WindowDataPair &dp : s_PlatformDataMap)
            if (dp.Window == window)
                return dp.Data;
        TKIT_FATAL("[ONYX][IMGUI] Platform data not found for window");
    }
    return ImGui::GetCurrentContext() ? static_cast<Platform_ContextData *>(ImGui::GetIO().BackendPlatformUserData)
                                      : nullptr;
}

static void platform_WindowFocusCallback(GLFWwindow *window, const i32 focused)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkWindowFocus && window == pdata->Window->GetHandle())
        pdata->UserCbkWindowFocus(window, focused);

    // Workaround for Linux: when losing focus with MouseIgnoreButtonUpWaitForFocusLoss set, we will temporarily ignore
    // subsequent Mouse Up events
    pdata->MouseIgnoreButtonUp = (pdata->MouseIgnoreButtonUpWaitForFocusLoss && focused == 0);
    pdata->MouseIgnoreButtonUpWaitForFocusLoss = false;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddFocusEvent(focused != 0);
}

static void platform_CursorPosCallback(GLFWwindow *window, f64 x, f64 y)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkCursorPos && window == pdata->Window->GetHandle())
        pdata->UserCbkCursorPos(window, x, y);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        i32 winx;
        i32 winy;
        glfwGetWindowPos(window, &winx, &winy);
        x += static_cast<f64>(winx);
        y += static_cast<f64>(winy);
    }

    io.AddMousePosEvent(static_cast<f32>(x), static_cast<f32>(y));
    pdata->LastValidMousePos = f32v2(static_cast<f32>(x), static_cast<f32>(y));
}

// Workaround: X11 seems to send spurious Leave/Enter events which would make us lose our position,
// so we back it up and restore on Leave/Enter (see https://github.com/ocornut/imgui/issues/4984)
static void platform_CursorEnterCallback(GLFWwindow *window, const i32 entered)
{
    Platform_ContextData *pdata = platform_GetData(window);
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
        pdata->MouseWindow = nullptr;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

static void platform_UpdateKeyModifiers(ImGuiIO &io, GLFWwindow *window)
{
    io.AddKeyEvent(ImGuiMod_Ctrl, (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
                                      (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Shift, (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                                       (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Alt, (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) ||
                                     (glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS));
    io.AddKeyEvent(ImGuiMod_Super, (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS) ||
                                       (glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS));
}
static void platform_MouseButtonCallback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkMousebutton && window == pdata->Window->GetHandle())
        pdata->UserCbkMousebutton(window, button, action, mods);

    // Workaround for Linux: ignore mouse up events which are following an focus loss following a viewport creation
    if (pdata->MouseIgnoreButtonUp && action == GLFW_RELEASE)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    platform_UpdateKeyModifiers(io, window);
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
}

static void platform_ScrollCallback(GLFWwindow *window, const f64 xoffset, const f64 yoffset)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkScroll && window == pdata->Window->GetHandle())
        pdata->UserCbkScroll(window, xoffset, yoffset);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddMouseWheelEvent(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
}

static i32 platform_TranslateUntranslatedKey(i32 key, const i32 scancode)
{
    // GLFW 3.1+ attempts to "untranslate" keys, which goes the opposite of what every other framework does, making
    // using lettered shortcuts difficult. (It had reasons to do so: namely GLFW is/was more likely to be used for
    // WASD-type game controls rather than lettered shortcuts, but IHMO the 3.1 change could have been done differently)
    // See https://github.com/glfw/glfw/issues/1502 for details.
    // Adding a workaround to undo this (so our keys are translated->untranslated->translated, likely a lossy process).
    // This won't cover edge cases but this is at least going to cover common cases.
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
        return key;

    const GLFWerrorfun perror = glfwSetErrorCallback(nullptr);
    const char *keyName = glfwGetKeyName(key, scancode);
    glfwSetErrorCallback(perror);
#ifdef ONYX_GLFW_GETERROR
    TKIT_UNUSED(glfwGetError(nullptr));
#endif
    if (keyName && keyName[0] != 0 && keyName[1] == 0)
    {
        const char charNames[] = "`-=[]\\,;\'./";
        const i32 charKeys[] = {GLFW_KEY_GRAVE_ACCENT,  GLFW_KEY_MINUS,     GLFW_KEY_EQUAL, GLFW_KEY_LEFT_BRACKET,
                                GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_COMMA, GLFW_KEY_SEMICOLON,
                                GLFW_KEY_APOSTROPHE,    GLFW_KEY_PERIOD,    GLFW_KEY_SLASH, 0};

        if (keyName[0] >= '0' && keyName[0] <= '9')
            key = GLFW_KEY_0 + (keyName[0] - '0');
        else if (keyName[0] >= 'A' && keyName[0] <= 'Z')
            key = GLFW_KEY_A + (keyName[0] - 'A');
        else if (keyName[0] >= 'a' && keyName[0] <= 'z')
            key = GLFW_KEY_A + (keyName[0] - 'a');
        else if (const char *p = strchr(charNames, keyName[0]))
            key = charKeys[p - charNames];
    }
    return key;
}

static void platform_KeyCallback(GLFWwindow *window, i32 keycode, const i32 scancode, const i32 action, const i32 mods)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkKey && window == pdata->Window->GetHandle())
        pdata->UserCbkKey(window, keycode, scancode, action, mods);

    if (action != GLFW_PRESS && action != GLFW_RELEASE)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    platform_UpdateKeyModifiers(io, window);

    if (keycode >= 0 && keycode < GLFW_KEY_LAST)
        pdata->KeyOwnerWindows[keycode] = (action == GLFW_PRESS) ? window : nullptr;

    keycode = platform_TranslateUntranslatedKey(keycode, scancode);
    const ImGuiKey imkey = platform_KeyToImGuiKey(keycode);
    io.AddKeyEvent(imkey, (action == GLFW_PRESS));
    io.SetKeyEventNativeData(imkey, keycode, scancode); // To support legacy indexing (<1.87 user code)
}

static void platform_CharCallback(GLFWwindow *window, const u32 code)
{
    Platform_ContextData *pdata = platform_GetData(window);
    if (pdata->UserCbkChar && window == pdata->Window->GetHandle())
        pdata->UserCbkChar(window, code);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddInputCharacter(code);
}

static void platform_InstallCallbacks(Platform_ContextData *pdata, GLFWwindow *window)
{
    pdata->UserCbkWindowFocus = glfwSetWindowFocusCallback(window, platform_WindowFocusCallback);
    pdata->UserCbkCursorEnter = glfwSetCursorEnterCallback(window, platform_CursorEnterCallback);
    pdata->UserCbkCursorPos = glfwSetCursorPosCallback(window, platform_CursorPosCallback);
    pdata->UserCbkMousebutton = glfwSetMouseButtonCallback(window, platform_MouseButtonCallback);
    pdata->UserCbkScroll = glfwSetScrollCallback(window, platform_ScrollCallback);
    pdata->UserCbkKey = glfwSetKeyCallback(window, platform_KeyCallback);
    pdata->UserCbkChar = glfwSetCharCallback(window, platform_CharCallback);
}

f32 platform_GetContentScaleForMonitor(GLFWmonitor *monitor)
{
#ifdef ONYX_GLFW_WAYLAND
    if (s_PlatformData.IsWayland)
        return 1.f;
#endif
#if defined(ONYX_GLFW_PER_MONITOR_DPI) && !(defined(TKIT_OS_APPLE) || defined(TKIT_OS_ANDROID))
    f32 xscale;
    f32 yscale;

    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
#else
    TKIT_UNUSED(monitor);
    return 1.f;
#endif
}

static void platform_UpdateMonitors()
{
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

    i32 mcount = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&mcount);

    bool updated_monitors = false;
    for (i32 n = 0; n < mcount; n++)
    {
        ImGuiPlatformMonitor monitor;
        i32 x, y;
        glfwGetMonitorPos(monitors[n], &x, &y);
        const GLFWvidmode *vmode = glfwGetVideoMode(monitors[n]);
        if (!vmode)
            continue;
        if (vmode->width <= 0 || vmode->height <= 0)
            continue;

        monitor.MainPos = monitor.WorkPos = ImVec2((f32)x, (f32)y);
        monitor.MainSize = monitor.WorkSize = ImVec2((f32)vmode->width, (f32)vmode->height);
#ifdef ONYX_GLFW_MONITOR_WORK_AREA
        i32 w, h;
        glfwGetMonitorWorkarea(monitors[n], &x, &y, &w, &h);
        if (w > 0 && h > 0)
        {
            monitor.WorkPos = ImVec2((f32)x, (f32)y);
            monitor.WorkSize = ImVec2((f32)w, (f32)h);
        }
#endif
        const f32 scale = platform_GetContentScaleForMonitor(monitors[n]);
        if (scale == 0.f)
            continue;
        monitor.DpiScale = scale;
        monitor.PlatformHandle = static_cast<void *>(monitors[n]); // [...] GLFW doc states: "guaranteed to be valid
                                                                   // only until the monitor configuration changes"

        if (!updated_monitors)
            pio.Monitors.resize(0);
        updated_monitors = true;
        pio.Monitors.push_back(monitor);
    }
}

static void platform_Init(Window *window)
{
    ImGuiIO &io = ImGui::GetIO();
    TKIT_ASSERT(!io.BackendPlatformUserData, "[ONYX][IMGUI] Platform backend is already initialized");

    TKit::TierAllocator *tier = TKit::GetTier();
    Platform_ContextData *pdata = tier->Create<Platform_ContextData>();

    pdata->Context = ImGui::GetCurrentContext();
    pdata->Window = window;

    s_PlatformDataMap.Append(window->GetHandle(), pdata);

    io.BackendPlatformUserData = static_cast<void *>(pdata);
    io.BackendPlatformName = "imgui_impl_onyx";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;

    //     bool hasViewports = true;
    // #ifdef ONYX_GLFW_GETPLATFORM
    //     hasViewports = glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    // #endif
    //     TKIT_LOG_INFO_IF(hasViewports, "[ONYX][IMGUI] Platform has viewports");
    //     TKIT_LOG_WARNING_IF(!hasViewports, "[ONYX][IMGUI] Platform does not have viewports");
    //     if (hasViewports)
    //         io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;

#if defined(ONYX_GLFW_MOUSE_PASSTHROUGH) || defined(ONYX_GLFW_WINDOW_HOVERED)
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
#endif

    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
#if ONYX_GLFW_VERSION_COMBINED >= 3300
    pio.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *text) { glfwSetClipboardString(nullptr, text); };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext *) { return glfwGetClipboardString(nullptr); };
#else
    pio.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *text) {
        glfwSetClipboardString(platform_GetData()->Window->GetHandle(), text);
    };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext *) {
        return glfwGetClipboardString(platform_GetData()->Window->GetHandle());
    };
#endif

    platform_InstallCallbacks(pdata, window->GetHandle());
    platform_UpdateMonitors();

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    mviewport->PlatformHandle = static_cast<void *>(window);

    // if (hasViewports)
    //     initMultiViewportSupport();
}

static void platform_GetWindowSizeAndFramebufferScale(GLFWwindow *window, ImVec2 *size, ImVec2 *fbscale)
{
    i32 w;
    i32 h;

    i32 wdisp;
    i32 hdisp;

    glfwGetWindowSize(window, &w, &h);
    glfwGetFramebufferSize(window, &wdisp, &hdisp);

    f32 fbscx = (w > 0) ? (f32)wdisp / (f32)w : 1.f;
    f32 fbscy = (h > 0) ? (f32)hdisp / (f32)h : 1.f;
#ifdef ONYX_GLFW_WAYLAND
    if (!s_PlatformData.IsWayland)
    {
        fbscx = 1.f;
        fbscy = 1.f;
    }
#endif
    if (size)
        *size = ImVec2((f32)w, (f32)h);
    if (fbscale)
        *fbscale = ImVec2(fbscx, fbscy);
}

static void platform_UpdateMouseData(Platform_ContextData *pdata, ImGuiIO &io)
{
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

    ImGuiID mouseViewportId = 0;
    const ImVec2 prevmpos = io.MousePos;
    for (i32 n = 0; n < pio.Viewports.Size; n++)
    {
        ImGuiViewport *viewport = pio.Viewports[n];
        GLFWwindow *window = static_cast<Window *>(viewport->PlatformHandle)->GetHandle();

        const bool windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
        if (windowFocused)
        {
            // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when
            // io.ConfigNavMoveSetMousePos is enabled by user) When multi-viewports are enabled, all Dear ImGui
            // positions are same as OS positions.

            if (io.WantSetMousePos)
                glfwSetCursorPos(window, (f64)(prevmpos.x - viewport->Pos.x), (f64)(prevmpos.y - viewport->Pos.y));

            // (Optional) Fallback to provide mouse position when focused (ImGui_ImplGlfw_CursorPosCallback already
            // provides this when hovered or captured)
            if (!pdata->MouseWindow)
            {
                f64 mx;
                f64 my;
                glfwGetCursorPos(window, &mx, &my);
                if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                {
                    // Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the
                    // mouse is on the upper-left corner of the app window) Multi-viewport mode: mouse position in OS
                    // absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary
                    // monitor)
                    i32 winx;
                    i32 winy;
                    glfwGetWindowPos(window, &winx, &winy);
                    mx += winx;
                    my += winy;
                }
                pdata->LastValidMousePos = f32v2((f32)mx, (f32)my);
                io.AddMousePosEvent((f32)mx, (f32)my);
            }
        }

        // (Optional) When using multiple viewports: call io.AddMouseViewportEvent() with the viewport the OS mouse
        // cursor is hovering. If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, Dear imGui will
        // ignore this field and infer the information using its flawed heuristic.
        // - [X] GLFW >= 3.3 backend ON WINDOWS ONLY does correctly ignore viewports with the _NoInputs flag (since we
        // implement hit via our WndProc hook)
        //       On other platforms we rely on the library fallbacking to its own search when reporting a viewport with
        //       _NoInputs flag.
        // - [!] GLFW <= 3.2 backend CANNOT correctly ignore viewports with the _NoInputs flag, and CANNOT reported
        // Hovered Viewport because of mouse capture.
        //       Some backend are not able to handle that correctly. If a backend report an hovered viewport that has
        //       the _NoInputs flag (e.g. when dragging a window for docking, the viewport has the _NoInputs flag in
        //       order to allow us to find the viewport under), then Dear ImGui is forced to ignore the value reported
        //       by the backend, and use its flawed heuristic to guess the viewport behind.
        // - [X] GLFW backend correctly reports this regardless of another viewport behind focused and dragged from (we
        // need this to find a useful drag and drop target).
        // FIXME: This is currently only correct on Win32. See what we do below with the WM_NCHITTEST, missing an
        // equivalent for other systems. See https://github.com/glfw/glfw/issues/1236 if you want to help in making this
        // a GLFW feature.
#ifdef ONYX_GLFW_MOUSE_PASSTHROUGH
        const bool winNoInput = (viewport->Flags & ImGuiViewportFlags_NoInputs) != 0;
        glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, winNoInput);
#endif
#if defined(ONYX_GLFW_MOUSE_PASSTHROUGH) || defined(ONYX_GLFW_WINDOW_HOVERED)
        if (glfwGetWindowAttrib(window, GLFW_HOVERED))
            mouseViewportId = viewport->ID;
#endif
    }

    if (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport)
        io.AddMouseViewportEvent(mouseViewportId);
}

static void platform_UpdateMouseCursor(Platform_ContextData *pdata, ImGuiIO &io)
{
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) ||
        glfwGetInputMode(pdata->Window->GetHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
        return;

    const ImGuiMouseCursor imcursor = ImGui::GetMouseCursor();
    const ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    for (i32 n = 0; n < pio.Viewports.Size; n++)
    {
        GLFWwindow *window = static_cast<Window *>(pio.Viewports[n]->PlatformHandle)->GetHandle();
        if (imcursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
        {
            if (pdata->LastMouseCursor != nullptr)
            {
                // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                pdata->LastMouseCursor = nullptr;
            }
        }
        else
        {
            // Show OS mouse cursor
            // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works
            // here.
            GLFWcursor *cursor = s_PlatformData.MouseCursors[imcursor]
                                     ? s_PlatformData.MouseCursors[imcursor]
                                     : s_PlatformData.MouseCursors[ImGuiMouseCursor_Arrow];
            if (pdata->LastMouseCursor != cursor)
            {
                glfwSetCursor(window, cursor);
                pdata->LastMouseCursor = cursor;
            }
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

static void platform_NewFrame()
{
    Platform_ContextData *pdata = platform_GetData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] Platform data has not been initialized");

    ImGuiIO &io = ImGui::GetIO();
    platform_GetWindowSizeAndFramebufferScale(pdata->Window->GetHandle(), &io.DisplaySize, &io.DisplayFramebufferScale);
    platform_UpdateMonitors();

    f64 time = glfwGetTime();
    if (time < pdata->Time)
        time = pdata->Time + 1.f / 60.f;

    io.DeltaTime = static_cast<f32>(time - pdata->Time);
    pdata->Time = time;

    pdata->MouseIgnoreButtonUp = false;
    platform_UpdateMouseData(pdata, io);
    platform_UpdateMouseCursor(pdata, io);

    TKIT_ASSERT((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0,
                "[ONYX][IMGUI] Gamepad controls are not supported");
}

static void platform_RestoreCallbacks(Platform_ContextData *pdata)
{
    GLFWwindow *window = pdata->Window->GetHandle();
    glfwSetWindowFocusCallback(window, pdata->UserCbkWindowFocus);
    glfwSetCursorEnterCallback(window, pdata->UserCbkCursorEnter);
    glfwSetCursorPosCallback(window, pdata->UserCbkCursorPos);
    glfwSetMouseButtonCallback(window, pdata->UserCbkMousebutton);
    glfwSetScrollCallback(window, pdata->UserCbkScroll);
    glfwSetKeyCallback(window, pdata->UserCbkKey);
    glfwSetCharCallback(window, pdata->UserCbkChar);
}

static void platform_Shutdown()
{
    Platform_ContextData *pdata = platform_GetData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] No platform data to shutdown");

    platform_RestoreCallbacks(pdata);

    ImGuiIO &io = ImGui::GetIO();
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &=
        ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad |
          ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport);
    pio.ClearPlatformHandlers();

    for (u32 i = 0; i < s_PlatformDataMap.GetSize(); ++i)
    {
        const WindowDataPair &pair = s_PlatformDataMap[i];
        if (pair.Window == pdata->Window->GetHandle())
            s_PlatformDataMap.RemoveUnordered(s_PlatformDataMap.begin() + i);
    }
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
    VKit::Sampler Sampler;
    VKit::DescriptorSetLayout DescriptorSetLayout{};
    VKit::PipelineLayout PipelineLayout{};
    VKit::GraphicsPipeline Pipeline{};

    VKit::Shader VertexShader{};
    VKit::Shader FragmentShader{};
};
static TKit::Storage<Renderer_GlobalData> s_RendererData{};

struct Renderer_Buffers
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
};

struct Renderer_ViewportData
{
    TKit::TierArray<Renderer_Buffers> Buffers{};
    u32 Index = 0;
};

struct Renderer_Texture
{
    VKit::DeviceImage Image{};
    VkDescriptorSet Set;
};

ONYX_NO_DISCARD static Result<Renderer_ViewportData *> renderer_CreateViewportData(const u32 imageCount)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Renderer_ViewportData *data = tier->Create<Renderer_ViewportData>();
    const auto cleanup = [&] {
        for (Renderer_Buffers &buffers : data->Buffers)
        {
            buffers.VertexBuffer.Destroy();
            buffers.IndexBuffer.Destroy();
        }
        tier->Destroy(data);
    };

    for (u32 i = 0; i < imageCount; ++i)
    {
        Renderer_Buffers &buffers = data->Buffers.Append();
        auto result = Resources::CreateBuffer<ImDrawVert>(Buffer_HostVertex);
        TKIT_RETURN_ON_ERROR(result, cleanup());
        buffers.VertexBuffer = result.GetValue();

        result = Resources::CreateBuffer<ImDrawIdx>(Buffer_HostIndex);
        TKIT_RETURN_ON_ERROR(result, cleanup());
        buffers.IndexBuffer = result.GetValue();
    }
    return data;
}

static void renderer_DestroyViewportData(Renderer_ViewportData *data)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (Renderer_Buffers &buffers : data->Buffers)
    {
        buffers.VertexBuffer.Destroy();
        buffers.IndexBuffer.Destroy();
    }
    tier->Destroy(data);
}

ONYX_NO_DISCARD static Result<> renderer_CreateShaders()
{
    TKIT_ASSERT(!s_RendererData->VertexShader, "[ONYX][IMGUI] Vertex shader is already initialized");
    TKIT_ASSERT(!s_RendererData->FragmentShader, "[ONYX][IMGUI] Fragment shader is already initialized");

    const auto &device = Core::GetDevice();

    auto result = VKit::Shader::Create(device, s_VertexShader, sizeof(s_VertexShader));
    TKIT_RETURN_ON_ERROR(result);
    s_RendererData->VertexShader = result.GetValue();

    result = VKit::Shader::Create(device, s_FragmentShader, sizeof(s_FragmentShader));
    TKIT_RETURN_ON_ERROR(result);
    s_RendererData->FragmentShader = result.GetValue();
    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> renderer_CreatePipeline(
    const VkPipelineRenderingCreateInfoKHR &pipInfo)
{
    return VKit::GraphicsPipeline::Builder(Core::GetDevice(), s_RendererData->PipelineLayout, pipInfo)
        .AddShaderStage(s_RendererData->VertexShader, VK_SHADER_STAGE_VERTEX_BIT)
        .AddShaderStage(s_RendererData->FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBindingDescription<ImDrawVert>()
        .AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos))
        .AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv))
        .AddAttributeDescription(0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col))
        .BeginColorAttachment()
        .EnableBlending()
        .EndColorAttachment()
        .SetViewportCount(1)
        .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .Bake()
        .Build();
}

ONYX_NO_DISCARD static Result<> renderer_CreateDeviceObjects(const VkPipelineRenderingCreateInfoKHR &pipInfo)
{
    const auto &device = Core::GetDevice();
    // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or
    // 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.

    {
        const auto result = VKit::Sampler::Builder(device).SetLodRange(-1000.f, 1000.f).Build();
        s_RendererData->Sampler = result.GetValue();
    }

    {
        const auto result = VKit::DescriptorSetLayout::Builder(device)
                                .AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .Build();
        TKIT_RETURN_ON_ERROR(result);
        s_RendererData->DescriptorSetLayout = result.GetValue();
    }

    {
        const auto result = VKit::PipelineLayout::Builder(device)
                                .AddDescriptorSetLayout(s_RendererData->DescriptorSetLayout)
                                .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 4 * sizeof(float))
                                .Build();
        s_RendererData->PipelineLayout = result.GetValue();
    }

    {
        TKIT_RETURN_IF_FAILED(renderer_CreateShaders());

        const auto result = renderer_CreatePipeline(pipInfo);
        TKIT_RETURN_ON_ERROR(result);
        s_RendererData->Pipeline = result.GetValue();
    }

    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<> renderer_Init(const u32 imageCount)
{
    ImGuiIO &io = ImGui::GetIO();

    io.BackendRendererName = "imgui_impl_onyx";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing
                                                               // for large meshes.
    io.BackendFlags |=
        ImGuiBackendFlags_RendererHasTextures; // We can honor ImGuiPlatformIO::Textures[] requests during render.
    // io.BackendFlags |=
    //     ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)

    const auto result = renderer_CreateViewportData(imageCount);
    TKIT_RETURN_ON_ERROR(result);
    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    mviewport->RendererUserData = result.GetValue();

    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<VkDescriptorSet> renderer_AddTexture(const VKit::DeviceImage &image)
{
    const auto &device = Core::GetDevice();
    const auto result = Descriptors::GetDescriptorPool().Allocate(s_RendererData->DescriptorSetLayout);
    TKIT_RETURN_ON_ERROR(result);

    VKit::DescriptorSet::Writer writer{device, &s_RendererData->DescriptorSetLayout};

    VkDescriptorImageInfo info{};
    info.sampler = s_RendererData->Sampler;
    info.imageView = image.GetImageView();
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    writer.WriteImage(0, info);

    const VKit::DescriptorSet &set = result.GetValue();
    writer.Overwrite(set);
    return set.GetHandle();
}

static void renderer_DestroyTexture(ImTextureData *tex)
{
    Renderer_Texture *bckTex = reinterpret_cast<Renderer_Texture *>(tex->BackendUserData);
    TKIT_ASSERT(bckTex, "[ONYX][IMGUI] Texture has no backend counterpart");
    const VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(tex->TexID);
    TKIT_ASSERT(bckTex->Set == set, "[ONYX][IMGUI] Backend texture descriptor set mismatch");

    // TKIT_RETURN_IF_FAILED(Descriptors::GetDescriptorPool().Deallocate(set));

    bckTex->Image.Destroy();
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(bckTex);

    tex->SetTexID(ImTextureID_Invalid);
    tex->BackendUserData = nullptr;
    tex->SetStatus(ImTextureStatus_Destroyed);
}

ONYX_NO_DISCARD static Result<> renderer_UpdateTexture(ImTextureData *tex, const u32 imageCount)
{
    const auto &device = Core::GetDevice();
    const VmaAllocator allocator = Core::GetVulkanAllocator();
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        // IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        TKIT_ASSERT(tex->TexID == ImTextureID_Invalid && !tex->BackendUserData);
        TKIT_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        const auto imgresult =
            VKit::DeviceImage::Builder(
                device, allocator, VkExtent2D{static_cast<u32>(tex->Width), static_cast<u32>(tex->Height)},
                VK_FORMAT_R8G8B8A8_UNORM,
                VKit::DeviceImageFlag_Sampled | VKit::DeviceImageFlag_Destination | VKit::DeviceImageFlag_Color)
                .WithImageView()
                .Build();

        TKIT_RETURN_ON_ERROR(imgresult);

        TKit::TierAllocator *tier = TKit::GetTier();
        Renderer_Texture *bckTex = tier->Create<Renderer_Texture>();
        bckTex->Image = imgresult.GetValue();

        const auto sresult = renderer_AddTexture(bckTex->Image);
        TKIT_RETURN_ON_ERROR(sresult);
        bckTex->Set = sresult.GetValue();

        // Store identifiers
        tex->SetTexID(reinterpret_cast<ImTextureID>(bckTex->Set));
        tex->BackendUserData = bckTex;
        TKIT_LOG_DEBUG("[ONYX][IMGUI] Created new texture with id '{}'", tex->GetTexID());
    }

    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
    {
        TKIT_LOG_DEBUG("[ONYX][IMGUI] Updating texture with id '{}'", tex->GetTexID());
        Renderer_Texture *bckTex = static_cast<Renderer_Texture *>(tex->BackendUserData);
        TKIT_ASSERT(bckTex->Image.GetBytesPerPixel() == static_cast<VkDeviceSize>(tex->BytesPerPixel),
                    "[ONYX][IMGUI] Bytes per pixel mismatch between VKit::DeviceImage and ImGui texture");

        // Update full texture or selected blocks. We only ever write to textures regions which have never been used
        // before! This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual
        // regions. We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.

        const bool wantCreate = tex->Status == ImTextureStatus_WantCreate;
        const u32 xupload = wantCreate ? 0 : tex->UpdateRect.x;
        const u32 yupload = wantCreate ? 0 : tex->UpdateRect.y;
        const u32 wupload = wantCreate ? tex->Width : tex->UpdateRect.w;
        const u32 hupload = wantCreate ? tex->Height : tex->UpdateRect.h;

        const VkDeviceSize wsize = wupload * tex->BytesPerPixel;
        const VkDeviceSize size = hupload * wsize;
        auto result =
            Resources::CreateBuffer(VKit::DeviceBufferFlag_Staging | VKit::DeviceBufferFlag_HostMapped, 1, size);
        TKIT_RETURN_ON_ERROR(result);

        VKit::DeviceBuffer &uploadBuffer = result.GetValue();
        std::byte *mem = static_cast<std::byte *>(uploadBuffer.GetData());
        for (u32 y = 0; y < hupload; ++y)
            TKit::ForwardCopy(mem + wsize * y,
                              tex->GetPixelsAt(static_cast<i32>(xupload), static_cast<i32>(yupload + y)), wsize);

        TKIT_RETURN_IF_FAILED(uploadBuffer.Flush());
        TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

        VKit::CommandPool &pool = Execution::GetTransientTransferPool();
        const auto cmdres = pool.BeginSingleTimeCommands();
        TKIT_RETURN_ON_ERROR(cmdres, uploadBuffer.Destroy());

        const VkCommandBuffer cmd = cmdres.GetValue();

        bckTex->Image.TransitionLayout2(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            {.DstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .DstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

        VkBufferImageCopy2KHR copy{};
        copy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent.width = wupload;
        copy.imageExtent.height = hupload;
        copy.imageExtent.depth = 1;
        copy.imageOffset.x = xupload;
        copy.imageOffset.y = yupload;

        bckTex->Image.CopyFromBuffer2(cmd, uploadBuffer, copy);

        bckTex->Image.TransitionLayout2(
            cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {.SrcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .SrcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

        const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        TKIT_RETURN_IF_FAILED(pool.EndSingleTimeCommands(cmd, queue->GetHandle()), uploadBuffer.Destroy());

        uploadBuffer.Destroy();

        tex->SetStatus(ImTextureStatus_OK);
    }

    if (tex->Status == ImTextureStatus_WantDestroy && static_cast<u32>(tex->UnusedFrames) >= imageCount)
        renderer_DestroyTexture(tex);
    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<> renderer_Render(const ImDrawData *ddata, const VkCommandBuffer cmd)
{
    const f32v2 fb =
        f32v2{ddata->DisplaySize.x * ddata->FramebufferScale.x, ddata->DisplaySize.y * ddata->FramebufferScale.y};

    if (fb[0] <= 0.f || fb[1] <= 0.f)
        return Result<>::Ok();

    Renderer_ViewportData *vdata = reinterpret_cast<Renderer_ViewportData *>(ddata->OwnerViewport->RendererUserData);
    TKIT_ASSERT(vdata, "[ONYX][IMGUI] Viewport ddata is null");
    TKIT_ASSERT(!vdata->Buffers.IsEmpty(),
                "[ONYX][IMGUI] Viewport ddata has no buffers. They should have been created prior to this call");

    if (ddata->Textures)
        for (ImTextureData *tex : *ddata->Textures)
            if (tex->Status != ImTextureStatus_OK)
            {
                TKIT_RETURN_IF_FAILED(renderer_UpdateTexture(tex, vdata->Buffers.GetSize()));
            }

    s_RendererData->Pipeline.Bind(cmd);

    vdata->Index = (vdata->Index + 1) % vdata->Buffers.GetSize();

    Renderer_Buffers &buffers = vdata->Buffers[vdata->Index];
    if (ddata->TotalVtxCount > 0)
    {
        const u32 vsize = static_cast<u32>(ddata->TotalVtxCount);
        const u32 isize = static_cast<u32>(ddata->TotalIdxCount);

        TKIT_RETURN_IF_FAILED(
            Resources::GrowBufferIfNeeded<ImDrawVert>(buffers.VertexBuffer, vsize, Buffer_HostVertex));
        TKIT_RETURN_IF_FAILED(Resources::GrowBufferIfNeeded<ImDrawIdx>(buffers.IndexBuffer, isize, Buffer_HostIndex));

        VkDeviceSize voffset = 0;
        VkDeviceSize ioffset = 0;
        for (const ImDrawList *dlist : ddata->CmdLists)
        {
            const VkDeviceSize lvsize = dlist->VtxBuffer.Size * sizeof(ImDrawVert);
            const VkDeviceSize lisize = dlist->IdxBuffer.Size * sizeof(ImDrawIdx);

            buffers.VertexBuffer.Write(dlist->VtxBuffer.Data, {.srcOffset = 0, .dstOffset = voffset, .size = lvsize});
            buffers.IndexBuffer.Write(dlist->IdxBuffer.Data, {.srcOffset = 0, .dstOffset = ioffset, .size = lisize});

            voffset += lvsize;
            ioffset += lisize;
        }

        TKIT_RETURN_IF_FAILED(buffers.VertexBuffer.Flush());
        TKIT_RETURN_IF_FAILED(buffers.IndexBuffer.Flush());

        buffers.VertexBuffer.BindAsVertexBuffer(cmd);
        buffers.IndexBuffer.BindAsIndexBuffer<ImDrawIdx>(cmd);
    }

    const auto table = Core::GetDeviceTable();
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = fb[0];
    viewport.height = fb[1];
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    table->CmdSetViewport(cmd, 0, 1, &viewport);

    struct PushConstant
    {
        f32v2 sc;
        f32v2 t;
    };

    PushConstant pc;
    pc.sc = f32v2{2.f / ddata->DisplaySize.x, 2.f / ddata->DisplaySize.y};
    pc.t = f32v2{-1.f - ddata->DisplayPos.x * pc.sc[0], -1.f - ddata->DisplayPos.y * pc.sc[1]};

    table->CmdPushConstants(cmd, s_RendererData->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant),
                            &pc);

    const f32v2 clipOff = f32v2{ddata->DisplayPos.x, ddata->DisplayPos.y};
    const f32v2 clipScale = f32v2{ddata->FramebufferScale.x, ddata->FramebufferScale.y};

    VkDescriptorSet lastSet = VK_NULL_HANDLE;

    u32 voffset = 0;
    u32 ioffset = 0;
    for (const ImDrawList *dlist : ddata->CmdLists)
    {
        for (u32 i = 0; i < static_cast<u32>(dlist->CmdBuffer.Size); ++i)
        {
            const ImDrawCmd &icmd = dlist->CmdBuffer[i];

            f32v2 clipMin{(icmd.ClipRect.x - clipOff[0]) * clipScale[0], (icmd.ClipRect.y - clipOff[1]) * clipScale[1]};
            f32v2 clipMax{(icmd.ClipRect.z - clipOff[0]) * clipScale[0], (icmd.ClipRect.w - clipOff[1]) * clipScale[1]};

            clipMin = Math::Max(clipMin, f32v2{0.f});
            clipMax = Math::Min(clipMax, fb);
            if (clipMax[0] <= clipMin[0] || clipMax[1] <= clipMin[1])
                continue;

            VkRect2D scissor;
            scissor.offset.x = static_cast<i32>(clipMin[0]);
            scissor.offset.y = static_cast<i32>(clipMin[1]);
            scissor.extent.width = static_cast<u32>(clipMax[0] - clipMin[0]);
            scissor.extent.height = static_cast<u32>(clipMax[1] - clipMin[1]);
            table->CmdSetScissor(cmd, 0, 1, &scissor);

            const VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(icmd.GetTexID());
            if (set != lastSet)
                table->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_RendererData->PipelineLayout, 0, 1,
                                             &set, 0, nullptr);
            lastSet = set;

            table->CmdDrawIndexed(cmd, icmd.ElemCount, 1, icmd.IdxOffset + ioffset, icmd.VtxOffset + voffset, 1);
        }
        voffset += static_cast<u32>(dlist->VtxBuffer.Size);
        ioffset += static_cast<u32>(dlist->IdxBuffer.Size);
    }

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = static_cast<u32>(fb[0]);
    scissor.extent.height = static_cast<u32>(fb[1]);
    table->CmdSetScissor(cmd, 0, 1, &scissor);
    return Result<>::Ok();
}

static void renderer_Shutdown()
{
    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    Renderer_ViewportData *vdata = reinterpret_cast<Renderer_ViewportData *>(mviewport->RendererUserData);

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

Result<> Create(Window *window)
{
    platform_Init(window);
    return renderer_Init(window->GetSwapChain().GetImageCount());
}

void Destroy()
{
    VKIT_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    renderer_Shutdown();
    platform_Shutdown();
    ImGui::DestroyPlatformWindows();
}

Result<> Initialize()
{
    const GLFWerrorfun perror = glfwSetErrorCallback(nullptr);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#ifdef ONYX_GLFW_NEW_CURSORS
    s_PlatformData.MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    s_PlatformData.MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#endif
    glfwSetErrorCallback(perror);
#ifdef ONYX_GLFW_GETERROR
    TKIT_UNUSED(glfwGetError(nullptr));
#endif

#ifdef ONYX_GLFW_WAYLAND
    s_PlatformData.IsWayland = platform_IsWayland();
#endif
    s_RendererData.Construct();
    return renderer_CreateDeviceObjects(Renderer::CreatePipelineRenderingCreateInfo());
}
void Terminate()
{
    s_RendererData->Pipeline.Destroy();
    s_RendererData->Sampler.Destroy();
    s_RendererData->PipelineLayout.Destroy();
    s_RendererData->DescriptorSetLayout.Destroy();
    s_RendererData->FragmentShader.Destroy();
    s_RendererData->VertexShader.Destroy();
    s_RendererData.Destruct();
    for (ImGuiMouseCursor cursor = 0; cursor < ImGuiMouseCursor_COUNT; cursor++)
        glfwDestroyCursor(s_PlatformData.MouseCursors[cursor]);
}

void NewFrame()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::NewFrame");
    platform_NewFrame();
    ImGui::NewFrame();
}

Result<> RenderData(ImDrawData *data, const VkCommandBuffer commandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::RenderDrawData");
    return renderer_Render(data, commandBuffer);
}
void RenderWindows()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::RenderWindows");
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

} // namespace Onyx::ImGuiBackend
