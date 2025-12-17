#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/api.hpp"

namespace Onyx
{
class Window;

/**
 * @brief The `Input` namespace handles all input events for the application.
 */
namespace Input
{
/**
 * @brief Poll events.
 *
 * Calls `glfwPollEvents()` under the hood.
 *
 */
ONYX_API void PollEvents();

/**
 * @brief Install the input callbacks for the given window.
 *
 * This method is called automatically by the Window class. It is not meant to be called by the user.
 *
 * @param p_Window The window to install the callbacks to.
 */
ONYX_API void InstallCallbacks(Window &p_Window);

/**
 * @brief An enum listing all the keys that can be used in the application.
 *
 */
enum class ONYX_API Key : u16
{
    Space,
    Apostrophe,
    Comma,
    Minus,
    Period,
    Slash,
    N0,
    N1,
    N2,
    N3,
    N4,
    N5,
    N6,
    N7,
    N8,
    N9,
    Semicolon,
    Equal,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    LeftBracket,
    Backslash,
    RightBracket,
    GraveAccent,
    World_1,
    World_2,
    Escape,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right,
    Left,
    Down,
    Up,
    PageUp,
    PageDown,
    Home,
    End,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    F25,
    KP_0,
    KP_1,
    KP_2,
    KP_3,
    KP_4,
    KP_5,
    KP_6,
    KP_7,
    KP_8,
    KP_9,
    KPDecimal,
    KPDivide,
    KPMultiply,
    KPSubtract,
    KPAdd,
    KPEnter,
    KPEqual,
    LeftShift,
    LeftControl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightControl,
    RightAlt,
    RightSuper,
    Menu,
    None
};

/**
 * @brief An enum listing all the mouse buttons that can be used in the application.
 *
 */
enum class Mouse : u8
{
    Button1,
    Button2,
    Button3,
    Button4,
    Button5,
    Button6,
    Button7,
    Button8,
    ButtonLast,
    ButtonLeft,
    ButtonRight,
    ButtonMiddle,
    None
};

/**
 * @brief Get the current mouse screen position, ranging between -1 and 1.
 *
 * The position follows a centered coordinate system, with the y axis pointing upwards. This coordinate system is
 * constant and is retrieved from the GLFW API, modified slightly so that it ranges between -1 and 1 and the y axis
 * points upwards.
 *
 * @param p_Window The window to get the mouse position from.
 * @return The mouse position.
 */
ONYX_API f32v2 GetScreenMousePosition(Window *p_Window);

/**
 * @brief Check if a key is currently pressed.
 *
 * @param p_Window The window to check the key for.
 * @param p_Key The key to check.
 * @return true if the key is currently pressed, false otherwise.
 */
ONYX_API bool IsKeyPressed(Window *p_Window, Key p_Key);

/**
 * @brief Check if a key was released in the current frame.
 *
 * @param p_Window The window to check the key for.
 * @param p_Key The key to check.
 * @return true if the key was released in the current frame, false otherwise.
 */
ONYX_API bool IsKeyReleased(Window *p_Window, Key p_Key);

/**
 * @brief Check if a mouse button is currently pressed.
 *
 * @param p_Window The window to check the button for.
 * @param p_Button The button to check.
 * @return true if the mouse button is currently pressed, false otherwise.
 */
ONYX_API bool IsMouseButtonPressed(Window *p_Window, Mouse p_Button);

/**
 * @brief Check if a mouse button was released in the current frame.
 *
 * @param p_Window The window to check the button for.
 * @param p_Button The button to check.
 * @return true if the mouse button was released in the current frame, false otherwise.
 */
ONYX_API bool IsMouseButtonReleased(Window *p_Window, Mouse p_Button);

/**
 * @brief Get the key enum value as a string.
 *
 * @param p_Key The key to get the name for.
 * @return The name of the key as a string.
 */
ONYX_API const char *GetKeyName(Key p_Key);
}; // namespace Input

/**
 * @brief A struct that represents an event that can be processed by the application.
 *
 * The event type is stored in the Type field, and the event data is stored in the corresponding fields.
 * Accessing the fields of the event should be done according to the Type field. Failure to do so may result in
 * undefined behaviour.
 *
 */
struct ONYX_API Event
{
    enum ActionType : u8
    {
        KeyPressed,
        KeyReleased,
        KeyRepeat,
        MouseMoved,
        MousePressed,
        MouseReleased,
        MouseEntered,
        MouseLeft,
        Scrolled,
        WindowMoved,
        WindowResized,
        WindowFocused,
        WindowUnfocused,
        WindowClosed,
        WindowMinimized,
        WindowRestored,
        FramebufferResized,
        CharInput,
    };

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
    ActionType Type;
    Input::Key Key;

    WindowMovedResized WindowDelta;

    MouseState Mouse;
    Char Character;
    f32v2 ScrollOffset{0.f};
    Window *Window = nullptr;

    operator bool() const
    {
        return !Empty;
    }
};
} // namespace Onyx
