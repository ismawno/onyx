#include "onyx/core/pch.hpp"
#include "onyx/imgui/backend.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/glfw.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/macros.hpp"
// #include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

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

namespace Onyx
{
struct ImGuiPlatformData
{
    ImGuiContext *Context;
    Window *Window;
    GLFWcursor *MouseCursors[ImGuiMouseCursor_COUNT];
    GLFWcursor *LastMouseCursor;

    GLFWwindow *MouseWindow;
    GLFWwindow *KeyOwnerWindows[GLFW_KEY_LAST];

    f64 Time = 0.f;

    bool MouseIgnoreButtonUpWaitForFocusLoss;
    bool MouseIgnoreButtonUp;
    bool IsWayland;

    f32v2 LastValidMousePos;

    GLFWwindowfocusfun UserCbkWindowFocus;
    GLFWcursorposfun UserCbkCursorPos;
    GLFWcursorenterfun UserCbkCursorEnter;
    GLFWmousebuttonfun UserCbkMousebutton;
    GLFWscrollfun UserCbkScroll;
    GLFWkeyfun UserCbkKey;
    GLFWcharfun UserCbkChar;
};

static bool isWayland()
{
#if !defined(ONYX_GLFW_WAYLAND)
    return false;
#elif defined(ONYX_GLFW_GETPLATFORM)
    return glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#else
    const char *version = glfwGetVersionString();
    if (!strstr(version, "Wayland")) // e.g. Ubuntu 22.04 ships with GLFW 3.3.6 compiled without Wayland
        return false;
#    ifdef GLFW_EXPOSE_NATIVE_X11
    if (glfwGetX11Display())
        return false;
#    endif
    return true;
#endif
}

ImGuiKey keyToImGuiKey(const i32 keycode)
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
    ImGuiPlatformData *Data;
};
static TKit::TierArray<WindowDataPair> s_PlatformDataMap{};

static ImGuiPlatformData *getPlatformData(GLFWwindow *window = nullptr)
{
    if (window)
    {
        for (const WindowDataPair &dp : s_PlatformDataMap)
            if (dp.Window == window)
                return dp.Data;
        TKIT_FATAL("[ONYX][IMGUI] Platform data not found for window");
    }
    return ImGui::GetCurrentContext() ? static_cast<ImGuiPlatformData *>(ImGui::GetIO().BackendPlatformUserData)
                                      : nullptr;
}

static void windowFocusCallback(GLFWwindow *window, const i32 focused)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
    if (pdata->UserCbkWindowFocus && window == pdata->Window->GetHandle())
        pdata->UserCbkWindowFocus(window, focused);

    // Workaround for Linux: when losing focus with MouseIgnoreButtonUpWaitForFocusLoss set, we will temporarily ignore
    // subsequent Mouse Up events
    pdata->MouseIgnoreButtonUp = (pdata->MouseIgnoreButtonUpWaitForFocusLoss && focused == 0);
    pdata->MouseIgnoreButtonUpWaitForFocusLoss = false;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddFocusEvent(focused != 0);
}

static void cursorPosCallback(GLFWwindow *window, f64 x, f64 y)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
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
static void cursorEnterCallback(GLFWwindow *window, const i32 entered)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
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

static void updateKeyModifiers(ImGuiIO &io, GLFWwindow *window)
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
static void mouseButtonCallback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
    if (pdata->UserCbkMousebutton && window == pdata->Window->GetHandle())
        pdata->UserCbkMousebutton(window, button, action, mods);

    // Workaround for Linux: ignore mouse up events which are following an focus loss following a viewport creation
    if (pdata->MouseIgnoreButtonUp && action == GLFW_RELEASE)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    updateKeyModifiers(io, window);
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
}

static void scrollCallback(GLFWwindow *window, const f64 xoffset, const f64 yoffset)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
    if (pdata->UserCbkScroll && window == pdata->Window->GetHandle())
        pdata->UserCbkScroll(window, xoffset, yoffset);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddMouseWheelEvent(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
}

static i32 translateUntranslatedKey(i32 key, const i32 scancode)
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

static void keyCallback(GLFWwindow *window, i32 keycode, const i32 scancode, const i32 action, const i32 mods)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
    if (pdata->UserCbkKey && window == pdata->Window->GetHandle())
        pdata->UserCbkKey(window, keycode, scancode, action, mods);

    if (action != GLFW_PRESS && action != GLFW_RELEASE)
        return;

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    updateKeyModifiers(io, window);

    if (keycode >= 0 && keycode < IM_COUNTOF(pdata->KeyOwnerWindows))
        pdata->KeyOwnerWindows[keycode] = (action == GLFW_PRESS) ? window : nullptr;

    keycode = translateUntranslatedKey(keycode, scancode);
    const ImGuiKey imkey = keyToImGuiKey(keycode);
    io.AddKeyEvent(imkey, (action == GLFW_PRESS));
    io.SetKeyEventNativeData(imkey, keycode, scancode); // To support legacy indexing (<1.87 user code)
}

static void charCallback(GLFWwindow *window, const u32 code)
{
    ImGuiPlatformData *pdata = getPlatformData(window);
    if (pdata->UserCbkChar && window == pdata->Window->GetHandle())
        pdata->UserCbkChar(window, code);

    ImGuiIO &io = ImGui::GetIO(pdata->Context);
    io.AddInputCharacter(code);
}

static void installCallbacks(ImGuiPlatformData *pdata, GLFWwindow *window)
{
    pdata->UserCbkWindowFocus = glfwSetWindowFocusCallback(window, windowFocusCallback);
    pdata->UserCbkCursorEnter = glfwSetCursorEnterCallback(window, cursorEnterCallback);
    pdata->UserCbkCursorPos = glfwSetCursorPosCallback(window, cursorPosCallback);
    pdata->UserCbkMousebutton = glfwSetMouseButtonCallback(window, mouseButtonCallback);
    pdata->UserCbkScroll = glfwSetScrollCallback(window, scrollCallback);
    pdata->UserCbkKey = glfwSetKeyCallback(window, keyCallback);
    pdata->UserCbkChar = glfwSetCharCallback(window, charCallback);
}

f32 getContentScaleForMonitor(GLFWmonitor *monitor)
{
#ifdef ONYX_GLFW_WAYLAND
    if (isWayland())
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

static void updateMonitors()
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
        const f32 scale = getContentScaleForMonitor(monitors[n]);
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

static void platformImGuiInit(Window *window)
{
    ImGuiIO &io = ImGui::GetIO();
    TKIT_ASSERT(!io.BackendPlatformUserData, "[ONYX][IMGUI] ImGui backend is already initialized");

    TKit::TierAllocator *tier = TKit::GetTier();
    ImGuiPlatformData *pdata = tier->Create<ImGuiPlatformData>();

    pdata->Context = ImGui::GetCurrentContext();
    pdata->Window = window;
    pdata->IsWayland = isWayland();

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
        glfwSetClipboardString(getPlatformData()->Window->GetHandle(), text);
    };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext *) {
        return glfwGetClipboardString(getPlatformData()->Window->GetHandle());
    };
#endif

    const GLFWerrorfun perror = glfwSetErrorCallback(nullptr);
    pdata->MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#ifdef ONYX_GLFW_NEW_CURSORS
    pdata->MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    pdata->MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#endif
    glfwSetErrorCallback(perror);
#ifdef ONYX_GLFW_GETERROR
    TKIT_UNUSED(glfwGetError(nullptr));
#endif

    installCallbacks(pdata, window->GetHandle());
    updateMonitors();

    ImGuiViewport *mviewport = ImGui::GetMainViewport();
    mviewport->PlatformHandle = static_cast<void *>(window);

    // if (hasViewports)
    //     initMultiViewportSupport();
}

static void getWindowSizeAndFramebufferScale(GLFWwindow *window, ImVec2 *size, ImVec2 *fbscale)
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
    const ImGuiPlatformData *pdata = getPlatformData();
    if (!pdata->IsWayland)
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

static void updateMouseData(ImGuiPlatformData *pdata, ImGuiIO &io)
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

static void updateMouseCursor(ImGuiPlatformData *pdata, ImGuiIO &io)
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
            GLFWcursor *cursor = pdata->MouseCursors[imcursor] ? pdata->MouseCursors[imcursor]
                                                               : pdata->MouseCursors[ImGuiMouseCursor_Arrow];
            if (pdata->LastMouseCursor != cursor)
            {
                glfwSetCursor(window, cursor);
                pdata->LastMouseCursor = cursor;
            }
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void platformImGuiNewFrame()
{
    ImGuiPlatformData *pdata = getPlatformData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] Platform data has not been initialized");

    ImGuiIO &io = ImGui::GetIO();
    getWindowSizeAndFramebufferScale(pdata->Window->GetHandle(), &io.DisplaySize, &io.DisplayFramebufferScale);
    updateMonitors();

    f64 time = glfwGetTime();
    if (time < pdata->Time)
        time = pdata->Time + 1.f / 60.f;

    io.DeltaTime = static_cast<f32>(time - pdata->Time);
    pdata->Time = time;

    pdata->MouseIgnoreButtonUp = false;
    updateMouseData(pdata, io);
    updateMouseCursor(pdata, io);

    TKIT_ASSERT((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0,
                "[ONYX][IMGUI] Gamepad controls are not supported");
}

static void restoreCallbacks(ImGuiPlatformData *pdata)
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

void platformImGuiShutdown()
{
    ImGuiPlatformData *pdata = getPlatformData();
    TKIT_ASSERT(pdata, "[ONYX][IMGUI] No platform data to shutdown");

    restoreCallbacks(pdata);
    for (ImGuiMouseCursor cursor = 0; cursor < ImGuiMouseCursor_COUNT; cursor++)
        glfwDestroyCursor(pdata->MouseCursors[cursor]);

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

void InitializeImGui(Window *window)
{
    platformImGuiInit(window);

    const VKit::Instance &instance = Core::GetInstance();
    const VKit::LogicalDevice &device = Core::GetDevice();

    const ImGuiIO &io = ImGui::GetIO();
    TKIT_LOG_WARNING_IF(
        (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) &&
            instance.GetInfo().Flags & VKit::InstanceFlag_HasValidationLayers,
        "[ONYX][IMGUI] Vulkan validation layers have become stricter regarding semaphore and fence usage when "
        "submitting to "
        "Execution. ImGui may not have caught up to this and may trigger validation errors when the "
        "ImGuiConfigFlags_ViewportsEnable flag is set. If this is the case, either disable the flag or the vulkan "
        "validation layers. If the application runs well, you may safely ignore this warning");

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
    pipelineInfo.PipelineRenderingCreateInfo = Renderer::CreatePipelineRenderingCreateInfo();
    pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    const VkSurfaceCapabilitiesKHR &sc = window->GetSwapChain().GetInfo().SupportDetails.Capabilities;

    const u32 imageCount = sc.minImageCount != sc.maxImageCount ? sc.minImageCount + 1 : sc.minImageCount;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = instance.GetInfo().ApiVersion;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = *device.GetInfo().PhysicalDevice;
    initInfo.Device = device;
    initInfo.Queue = Execution::FindSuitableQueue(VKit::Queue_Graphics)->GetHandle();
    initInfo.QueueFamily = Execution::GetFamilyIndex(VKit::Queue_Graphics);
    initInfo.DescriptorPoolSize = 100;
    initInfo.MinImageCount = sc.minImageCount;
    initInfo.ImageCount = imageCount;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain = pipelineInfo;

    TKIT_CHECK_RETURNS(ImGui_ImplVulkan_LoadFunctions(instance.GetInfo().ApiVersion,
                                                      [](const char *name, void *) -> PFN_vkVoidFunction {
                                                          return VKit::Vulkan::GetInstanceProcAddr(Core::GetInstance(),
                                                                                                   name);
                                                      }),
                       true, "[ONYX] Failed to load ImGui Vulkan functions");

    TKIT_CHECK_RETURNS(ImGui_ImplVulkan_Init(&initInfo), true,
                       "[ONYX] Failed to initialize ImGui Vulkan for window '{}'", window->GetName());
}

void NewImGuiFrame()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::NewFrame");
    ImGui_ImplVulkan_NewFrame();
    platformImGuiNewFrame();
    ImGui::NewFrame();
}

void RenderImGuiData(ImDrawData *data, const VkCommandBuffer commandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::RenderDrawData");
    ImGui_ImplVulkan_RenderDrawData(data, commandBuffer);
}
void RenderImGuiWindows()
{
    TKIT_PROFILE_NSCOPE("Onyx::ImGui::RenderWindows");
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

void ShutdownImGui()
{
    ONYX_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    ImGui_ImplVulkan_Shutdown();
    platformImGuiShutdown();
    ImGui::DestroyPlatformWindows();
}

} // namespace Onyx
