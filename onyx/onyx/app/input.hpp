#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/glm.hpp"
#include "onyx/core/alias.hpp"
#include "onyx/core/api.hpp"

#include "onyx/core/glfw.hpp"

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
ONYX_API void InstallCallbacks(Window &p_Window) noexcept;

/**
 * @brief An enum listing all the keys that can be used in the application.
 *
 */
enum class ONYX_API Key : u16
{
    Space = GLFW_KEY_SPACE,
    Apostrophe = GLFW_KEY_APOSTROPHE,
    Comma = GLFW_KEY_COMMA,
    Minus = GLFW_KEY_MINUS,
    Period = GLFW_KEY_PERIOD,
    Slash = GLFW_KEY_SLASH,
    N0 = GLFW_KEY_0,
    N1 = GLFW_KEY_1,
    N2 = GLFW_KEY_2,
    N3 = GLFW_KEY_3,
    N4 = GLFW_KEY_4,
    N5 = GLFW_KEY_5,
    N6 = GLFW_KEY_6,
    N7 = GLFW_KEY_7,
    N8 = GLFW_KEY_8,
    N9 = GLFW_KEY_9,
    Semicolon = GLFW_KEY_SEMICOLON,
    Equal = GLFW_KEY_EQUAL,
    A = GLFW_KEY_A,
    B = GLFW_KEY_B,
    C = GLFW_KEY_C,
    D = GLFW_KEY_D,
    E = GLFW_KEY_E,
    F = GLFW_KEY_F,
    G = GLFW_KEY_G,
    H = GLFW_KEY_H,
    I = GLFW_KEY_I,
    J = GLFW_KEY_J,
    K = GLFW_KEY_K,
    L = GLFW_KEY_L,
    M = GLFW_KEY_M,
    N = GLFW_KEY_N,
    O = GLFW_KEY_O,
    P = GLFW_KEY_P,
    Q = GLFW_KEY_Q,
    R = GLFW_KEY_R,
    S = GLFW_KEY_S,
    T = GLFW_KEY_T,
    U = GLFW_KEY_U,
    V = GLFW_KEY_V,
    W = GLFW_KEY_W,
    X = GLFW_KEY_X,
    Y = GLFW_KEY_Y,
    Z = GLFW_KEY_Z,
    LeftBracket = GLFW_KEY_LEFT_BRACKET,
    Backslash = GLFW_KEY_BACKSLASH,
    RightBracket = GLFW_KEY_RIGHT_BRACKET,
    GraveAccent = GLFW_KEY_GRAVE_ACCENT,
    World_1 = GLFW_KEY_WORLD_1,
    World_2 = GLFW_KEY_WORLD_2,
    Escape = GLFW_KEY_ESCAPE,
    Enter = GLFW_KEY_ENTER,
    Tab = GLFW_KEY_TAB,
    Backspace = GLFW_KEY_BACKSPACE,
    Insert = GLFW_KEY_INSERT,
    Delete = GLFW_KEY_DELETE,
    Right = GLFW_KEY_RIGHT,
    Left = GLFW_KEY_LEFT,
    Down = GLFW_KEY_DOWN,
    Up = GLFW_KEY_UP,
    PageUp = GLFW_KEY_PAGE_UP,
    PageDown = GLFW_KEY_PAGE_DOWN,
    Home = GLFW_KEY_HOME,
    End = GLFW_KEY_END,
    CapsLock = GLFW_KEY_CAPS_LOCK,
    ScrollLock = GLFW_KEY_SCROLL_LOCK,
    NumLock = GLFW_KEY_NUM_LOCK,
    PrintScreen = GLFW_KEY_PRINT_SCREEN,
    Pause = GLFW_KEY_PAUSE,
    F1 = GLFW_KEY_F1,
    F2 = GLFW_KEY_F2,
    F3 = GLFW_KEY_F3,
    F4 = GLFW_KEY_F4,
    F5 = GLFW_KEY_F5,
    F6 = GLFW_KEY_F6,
    F7 = GLFW_KEY_F7,
    F8 = GLFW_KEY_F8,
    F9 = GLFW_KEY_F9,
    F10 = GLFW_KEY_F10,
    F11 = GLFW_KEY_F11,
    F12 = GLFW_KEY_F12,
    F13 = GLFW_KEY_F13,
    F14 = GLFW_KEY_F14,
    F15 = GLFW_KEY_F15,
    F16 = GLFW_KEY_F16,
    F17 = GLFW_KEY_F17,
    F18 = GLFW_KEY_F18,
    F19 = GLFW_KEY_F19,
    F20 = GLFW_KEY_F20,
    F21 = GLFW_KEY_F21,
    F22 = GLFW_KEY_F22,
    F23 = GLFW_KEY_F23,
    F24 = GLFW_KEY_F24,
    F25 = GLFW_KEY_F25,
    KP_0 = GLFW_KEY_KP_0,
    KP_1 = GLFW_KEY_KP_1,
    KP_2 = GLFW_KEY_KP_2,
    KP_3 = GLFW_KEY_KP_3,
    KP_4 = GLFW_KEY_KP_4,
    KP_5 = GLFW_KEY_KP_5,
    KP_6 = GLFW_KEY_KP_6,
    KP_7 = GLFW_KEY_KP_7,
    KP_8 = GLFW_KEY_KP_8,
    KP_9 = GLFW_KEY_KP_9,
    KPDecimal = GLFW_KEY_KP_DECIMAL,
    KPDivide = GLFW_KEY_KP_DIVIDE,
    KPMultiply = GLFW_KEY_KP_MULTIPLY,
    KPSubtract = GLFW_KEY_KP_SUBTRACT,
    KPAdd = GLFW_KEY_KP_ADD,
    KPEnter = GLFW_KEY_KP_ENTER,
    KPEqual = GLFW_KEY_KP_EQUAL,
    LeftShift = GLFW_KEY_LEFT_SHIFT,
    LeftControl = GLFW_KEY_LEFT_CONTROL,
    LeftAlt = GLFW_KEY_LEFT_ALT,
    LeftSuper = GLFW_KEY_LEFT_SUPER,
    RightShift = GLFW_KEY_RIGHT_SHIFT,
    RightControl = GLFW_KEY_RIGHT_CONTROL,
    RightAlt = GLFW_KEY_RIGHT_ALT,
    RightSuper = GLFW_KEY_RIGHT_SUPER,
    Menu = GLFW_KEY_MENU,
    None = GLFW_KEY_LAST + 1
};

/**
 * @brief An enum listing all the mouse buttons that can be used in the application.
 *
 */
enum class Mouse : u8
{
    Button1 = GLFW_MOUSE_BUTTON_1,
    Button2 = GLFW_MOUSE_BUTTON_2,
    Button3 = GLFW_MOUSE_BUTTON_3,
    Button4 = GLFW_MOUSE_BUTTON_4,
    Button5 = GLFW_MOUSE_BUTTON_5,
    Button6 = GLFW_MOUSE_BUTTON_6,
    Button7 = GLFW_MOUSE_BUTTON_7,
    Button8 = GLFW_MOUSE_BUTTON_8,
    ButtonLast = GLFW_MOUSE_BUTTON_LAST,
    ButtonLeft = GLFW_MOUSE_BUTTON_LEFT,
    ButtonRight = GLFW_MOUSE_BUTTON_RIGHT,
    ButtonMiddle = GLFW_MOUSE_BUTTON_MIDDLE,
    None = GLFW_MOUSE_BUTTON_LAST + 1
};

/**
 * @brief Get the current mouse position, normalized between -1 and 1 with the screen dimensions.
 *
 * The position follows a centered coordinate system, with the y axis pointing downwards. This coordinate system is
 * constant and is retrieved directly from the GLFW API.
 *
 * @param p_Window The window to get the mouse position from.
 * @return The mouse position.
 */
ONYX_API fvec2 GetNativeMousePosition(Window *p_Window) noexcept;

/**
 * @brief Get the current mouse position, normalized between -1 and 1 with the screen dimensions.
 *
 * The position follows a centered coordinate system, with the y axis pointing upwards. This coordinate system is
 * constant and is retrieved directly from the GLFW API.
 *
 * @param p_Window The window to get the mouse position from.
 * @return The mouse position.
 */
ONYX_API fvec2 GetCartesianMousePosition(Window *p_Window) noexcept;

/**
 * @brief Check if a key is currently pressed.
 *
 * @param p_Window The window to check the key for.
 * @param p_Key The key to check.
 * @return true if the key is currently pressed, false otherwise.
 */
ONYX_API bool IsKeyPressed(Window *p_Window, Key p_Key) noexcept;

/**
 * @brief Check if a key was released in the current frame.
 *
 * @param p_Window The window to check the key for.
 * @param p_Key The key to check.
 * @return true if the key was released in the current frame, false otherwise.
 */
ONYX_API bool IsKeyReleased(Window *p_Window, Key p_Key) noexcept;

/**
 * @brief Check if a mouse button is currently pressed.
 *
 * @param p_Window The window to check the button for.
 * @param p_Button The button to check.
 * @return true if the mouse button is currently pressed, false otherwise.
 */
ONYX_API bool IsMouseButtonPressed(Window *p_Window, Mouse p_Button) noexcept;

/**
 * @brief Check if a mouse button was released in the current frame.
 *
 * @param p_Window The window to check the button for.
 * @param p_Button The button to check.
 * @return true if the mouse button was released in the current frame, false otherwise.
 */
ONYX_API bool IsMouseButtonReleased(Window *p_Window, Mouse p_Button) noexcept;

/**
 * @brief Get the key enum value as a string.
 *
 * @param p_Key The key to get the name for.
 * @return The name of the key as a string.
 */
ONYX_API const char *GetKeyName(Key p_Key) noexcept;
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
    /**
     * @brief An enum representing different action types for events.
     */
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
        WindowResized,
        WindowClosed,
        WindowOpened,
        WindowFocused,
        WindowUnfocused
    };

    /**
     * @brief Structure representing old and new dimensions when a window is resized.
     */
    struct WindowResizedDimensions
    {
        u32 OldWidth = 0;
        u32 OldHeight = 0;
        u32 NewWidth = 0;
        u32 NewHeight = 0;
    };

    /**
     * @brief Structure representing the mouse state including position and button pressed.
     */
    struct MouseState
    {
        fvec2 Position{0.f};
        Input::Mouse Button;
    };

    bool Empty = false;
    ActionType Type;
    Input::Key Key;

    WindowResizedDimensions WindowResize;
    MouseState Mouse;
    fvec2 ScrollOffset{0.f};
    Window *Window = nullptr;

    explicit(false) operator bool() const
    {
        return !Empty;
    }
};

} // namespace Onyx
