#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/reflection/reflect.hpp"

namespace Onyx
{
class Window;

namespace Input
{
void PollEvents();
void InstallCallbacks(Window &p_Window);

TKIT_REFLECT_DECLARE_ENUM(Key)
enum Key : u16
{
    Key_Space,
    Key_Apostrophe,
    Key_Comma,
    Key_Minus,
    Key_Period,
    Key_Slash,
    Key_N0,
    Key_N1,
    Key_N2,
    Key_N3,
    Key_N4,
    Key_N5,
    Key_N6,
    Key_N7,
    Key_N8,
    Key_N9,
    Key_Semicolon,
    Key_Equal,
    Key_A,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_W,
    Key_X,
    Key_Y,
    Key_Z,
    Key_LeftBracket,
    Key_Backslash,
    Key_RightBracket,
    Key_GraveAccent,
    Key_World_1,
    Key_World_2,
    Key_Escape,
    Key_Enter,
    Key_Tab,
    Key_Backspace,
    Key_Insert,
    Key_Delete,
    Key_Right,
    Key_Left,
    Key_Down,
    Key_Up,
    Key_PageUp,
    Key_PageDown,
    Key_Home,
    Key_End,
    Key_CapsLock,
    Key_ScrollLock,
    Key_NumLock,
    Key_PrintScreen,
    Key_Pause,
    Key_F1,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_F11,
    Key_F12,
    Key_F13,
    Key_F14,
    Key_F15,
    Key_F16,
    Key_F17,
    Key_F18,
    Key_F19,
    Key_F20,
    Key_F21,
    Key_F22,
    Key_F23,
    Key_F24,
    Key_F25,
    Key_KP_0,
    Key_KP_1,
    Key_KP_2,
    Key_KP_3,
    Key_KP_4,
    Key_KP_5,
    Key_KP_6,
    Key_KP_7,
    Key_KP_8,
    Key_KP_9,
    Key_KPDecimal,
    Key_KPDivide,
    Key_KPMultiply,
    Key_KPSubtract,
    Key_KPAdd,
    Key_KPEnter,
    Key_KPEqual,
    Key_LeftShift,
    Key_LeftControl,
    Key_LeftAlt,
    Key_LeftSuper,
    Key_RightShift,
    Key_RightControl,
    Key_RightAlt,
    Key_RightSuper,
    Key_Menu,
    Key_None,
    Key_Count
};

TKIT_REFLECT_DECLARE_ENUM(Mouse)
enum Mouse : u8
{
    Mouse_Button1,
    Mouse_Button2,
    Mouse_Button3,
    Mouse_Button4,
    Mouse_Button5,
    Mouse_Button6,
    Mouse_Button7,
    Mouse_Button8,
    Mouse_ButtonLast,
    Mouse_ButtonLeft,
    Mouse_ButtonRight,
    Mouse_ButtonMiddle,
    Mouse_None,
    Mouse_Count
};

/**
 * @brief Get the current mouse screen position, ranging between -1 and 1.
 *
 */
f32v2 GetScreenMousePosition(Window *p_Window);

bool IsKeyPressed(Window *p_Window, Key p_Key);

bool IsKeyReleased(Window *p_Window, Key p_Key);

bool IsMouseButtonPressed(Window *p_Window, Mouse p_Button);

bool IsMouseButtonReleased(Window *p_Window, Mouse p_Button);

const char *GetKeyName(Key p_Key);
}; // namespace Input
TKIT_REFLECT_DECLARE_ENUM(EventType)
enum EventType : u8
{
    Event_KeyPressed,
    Event_KeyReleased,
    Event_KeyRepeat,
    Event_MouseMoved,
    Event_MousePressed,
    Event_MouseReleased,
    Event_MouseEntered,
    Event_MouseLeft,
    Event_Scrolled,
    Event_WindowMoved,
    Event_WindowResized,
    Event_WindowFocused,
    Event_WindowUnfocused,
    Event_WindowClosed,
    Event_WindowMinimized,
    Event_WindowRestored,
    Event_FramebufferResized,
    Event_CharInput,
    Event_SwapChainRecreated,
    Event_Count,
};

struct Event
{
    struct WindowMovedResized
    {
        u32v2 Old;
        u32v2 New;
    };

    struct MouseState
    {
        f32v2 Position{0.f};
        Input::Mouse Button;
    };

    struct Char
    {
        u32 Codepoint;
    };

    bool Empty = false;
    EventType Type;
    Input::Key Key;

    WindowMovedResized WindowDelta;

    MouseState Mouse;
    Char Character;
    f32v2 ScrollOffset{0.f};

    operator bool() const
    {
        return !Empty;
    }
};
} // namespace Onyx
