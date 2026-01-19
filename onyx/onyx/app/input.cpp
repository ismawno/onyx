#include "onyx/core/pch.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/glfw.hpp"

namespace Onyx::Input
{
static Window *windowFromGLFW(GLFWwindow *p_Window)
{
    return static_cast<Window *>(glfwGetWindowUserPointer(p_Window));
}

static i32 toGlfw(const Key p_Key)
{
    switch (p_Key)
    {
    case Key_Space:
        return GLFW_KEY_SPACE;
    case Key_Apostrophe:
        return GLFW_KEY_APOSTROPHE;
    case Key_Comma:
        return GLFW_KEY_COMMA;
    case Key_Minus:
        return GLFW_KEY_MINUS;
    case Key_Period:
        return GLFW_KEY_PERIOD;
    case Key_Slash:
        return GLFW_KEY_SLASH;
    case Key_N0:
        return GLFW_KEY_0;
    case Key_N1:
        return GLFW_KEY_1;
    case Key_N2:
        return GLFW_KEY_2;
    case Key_N3:
        return GLFW_KEY_3;
    case Key_N4:
        return GLFW_KEY_4;
    case Key_N5:
        return GLFW_KEY_5;
    case Key_N6:
        return GLFW_KEY_6;
    case Key_N7:
        return GLFW_KEY_7;
    case Key_N8:
        return GLFW_KEY_8;
    case Key_N9:
        return GLFW_KEY_9;
    case Key_Semicolon:
        return GLFW_KEY_SEMICOLON;
    case Key_Equal:
        return GLFW_KEY_EQUAL;
    case Key_A:
        return GLFW_KEY_A;
    case Key_B:
        return GLFW_KEY_B;
    case Key_C:
        return GLFW_KEY_C;
    case Key_D:
        return GLFW_KEY_D;
    case Key_E:
        return GLFW_KEY_E;
    case Key_F:
        return GLFW_KEY_F;
    case Key_G:
        return GLFW_KEY_G;
    case Key_H:
        return GLFW_KEY_H;
    case Key_I:
        return GLFW_KEY_I;
    case Key_J:
        return GLFW_KEY_J;
    case Key_K:
        return GLFW_KEY_K;
    case Key_L:
        return GLFW_KEY_L;
    case Key_M:
        return GLFW_KEY_M;
    case Key_N:
        return GLFW_KEY_N;
    case Key_O:
        return GLFW_KEY_O;
    case Key_P:
        return GLFW_KEY_P;
    case Key_Q:
        return GLFW_KEY_Q;
    case Key_R:
        return GLFW_KEY_R;
    case Key_S:
        return GLFW_KEY_S;
    case Key_T:
        return GLFW_KEY_T;
    case Key_U:
        return GLFW_KEY_U;
    case Key_V:
        return GLFW_KEY_V;
    case Key_W:
        return GLFW_KEY_W;
    case Key_X:
        return GLFW_KEY_X;
    case Key_Y:
        return GLFW_KEY_Y;
    case Key_Z:
        return GLFW_KEY_Z;
    case Key_LeftBracket:
        return GLFW_KEY_LEFT_BRACKET;
    case Key_Backslash:
        return GLFW_KEY_BACKSLASH;
    case Key_RightBracket:
        return GLFW_KEY_RIGHT_BRACKET;
    case Key_GraveAccent:
        return GLFW_KEY_GRAVE_ACCENT;
    case Key_World_1:
        return GLFW_KEY_WORLD_1;
    case Key_World_2:
        return GLFW_KEY_WORLD_2;
    case Key_Escape:
        return GLFW_KEY_ESCAPE;
    case Key_Enter:
        return GLFW_KEY_ENTER;
    case Key_Tab:
        return GLFW_KEY_TAB;
    case Key_Backspace:
        return GLFW_KEY_BACKSPACE;
    case Key_Insert:
        return GLFW_KEY_INSERT;
    case Key_Delete:
        return GLFW_KEY_DELETE;
    case Key_Right:
        return GLFW_KEY_RIGHT;
    case Key_Left:
        return GLFW_KEY_LEFT;
    case Key_Down:
        return GLFW_KEY_DOWN;
    case Key_Up:
        return GLFW_KEY_UP;
    case Key_PageUp:
        return GLFW_KEY_PAGE_UP;
    case Key_PageDown:
        return GLFW_KEY_PAGE_DOWN;
    case Key_Home:
        return GLFW_KEY_HOME;
    case Key_End:
        return GLFW_KEY_END;
    case Key_CapsLock:
        return GLFW_KEY_CAPS_LOCK;
    case Key_ScrollLock:
        return GLFW_KEY_SCROLL_LOCK;
    case Key_NumLock:
        return GLFW_KEY_NUM_LOCK;
    case Key_PrintScreen:
        return GLFW_KEY_PRINT_SCREEN;
    case Key_Pause:
        return GLFW_KEY_PAUSE;
    case Key_F1:
        return GLFW_KEY_F1;
    case Key_F2:
        return GLFW_KEY_F2;
    case Key_F3:
        return GLFW_KEY_F3;
    case Key_F4:
        return GLFW_KEY_F4;
    case Key_F5:
        return GLFW_KEY_F5;
    case Key_F6:
        return GLFW_KEY_F6;
    case Key_F7:
        return GLFW_KEY_F7;
    case Key_F8:
        return GLFW_KEY_F8;
    case Key_F9:
        return GLFW_KEY_F9;
    case Key_F10:
        return GLFW_KEY_F10;
    case Key_F11:
        return GLFW_KEY_F11;
    case Key_F12:
        return GLFW_KEY_F12;
    case Key_F13:
        return GLFW_KEY_F13;
    case Key_F14:
        return GLFW_KEY_F14;
    case Key_F15:
        return GLFW_KEY_F15;
    case Key_F16:
        return GLFW_KEY_F16;
    case Key_F17:
        return GLFW_KEY_F17;
    case Key_F18:
        return GLFW_KEY_F18;
    case Key_F19:
        return GLFW_KEY_F19;
    case Key_F20:
        return GLFW_KEY_F20;
    case Key_F21:
        return GLFW_KEY_F21;
    case Key_F22:
        return GLFW_KEY_F22;
    case Key_F23:
        return GLFW_KEY_F23;
    case Key_F24:
        return GLFW_KEY_F24;
    case Key_F25:
        return GLFW_KEY_F25;
    case Key_KP_0:
        return GLFW_KEY_KP_0;
    case Key_KP_1:
        return GLFW_KEY_KP_1;
    case Key_KP_2:
        return GLFW_KEY_KP_2;
    case Key_KP_3:
        return GLFW_KEY_KP_3;
    case Key_KP_4:
        return GLFW_KEY_KP_4;
    case Key_KP_5:
        return GLFW_KEY_KP_5;
    case Key_KP_6:
        return GLFW_KEY_KP_6;
    case Key_KP_7:
        return GLFW_KEY_KP_7;
    case Key_KP_8:
        return GLFW_KEY_KP_8;
    case Key_KP_9:
        return GLFW_KEY_KP_9;
    case Key_KPDecimal:
        return GLFW_KEY_KP_DECIMAL;
    case Key_KPDivide:
        return GLFW_KEY_KP_DIVIDE;
    case Key_KPMultiply:
        return GLFW_KEY_KP_MULTIPLY;
    case Key_KPSubtract:
        return GLFW_KEY_KP_SUBTRACT;
    case Key_KPAdd:
        return GLFW_KEY_KP_ADD;
    case Key_KPEnter:
        return GLFW_KEY_KP_ENTER;
    case Key_KPEqual:
        return GLFW_KEY_KP_EQUAL;
    case Key_LeftShift:
        return GLFW_KEY_LEFT_SHIFT;
    case Key_LeftControl:
        return GLFW_KEY_LEFT_CONTROL;
    case Key_LeftAlt:
        return GLFW_KEY_LEFT_ALT;
    case Key_LeftSuper:
        return GLFW_KEY_LEFT_SUPER;
    case Key_RightShift:
        return GLFW_KEY_RIGHT_SHIFT;
    case Key_RightControl:
        return GLFW_KEY_RIGHT_CONTROL;
    case Key_RightAlt:
        return GLFW_KEY_RIGHT_ALT;
    case Key_RightSuper:
        return GLFW_KEY_RIGHT_SUPER;
    case Key_Menu:
        return GLFW_KEY_MENU;
    case Key_None:
        return GLFW_KEY_LAST + 1;
    case Key_Count:
        return GLFW_KEY_LAST + 1;
    }
    return GLFW_KEY_LAST + 1;
}

static i32 toGlfw(const Mouse p_Mouse)
{
    switch (p_Mouse)
    {
    case Mouse_Button1:
        return GLFW_MOUSE_BUTTON_1;
    case Mouse_Button2:
        return GLFW_MOUSE_BUTTON_2;
    case Mouse_Button3:
        return GLFW_MOUSE_BUTTON_3;
    case Mouse_Button4:
        return GLFW_MOUSE_BUTTON_4;
    case Mouse_Button5:
        return GLFW_MOUSE_BUTTON_5;
    case Mouse_Button6:
        return GLFW_MOUSE_BUTTON_6;
    case Mouse_Button7:
        return GLFW_MOUSE_BUTTON_7;
    case Mouse_Button8:
        return GLFW_MOUSE_BUTTON_8;
    case Mouse_ButtonLast:
        return GLFW_MOUSE_BUTTON_LAST;
    case Mouse_ButtonLeft:
        return GLFW_MOUSE_BUTTON_LEFT;
    case Mouse_ButtonRight:
        return GLFW_MOUSE_BUTTON_RIGHT;
    case Mouse_ButtonMiddle:
        return GLFW_MOUSE_BUTTON_MIDDLE;
    case Mouse_None:
        return GLFW_MOUSE_BUTTON_LAST + 1;
    case Mouse_Count:
        return GLFW_MOUSE_BUTTON_LAST + 1;
    }
    return GLFW_MOUSE_BUTTON_LAST + 1;
}

static Key toKey(const i32 p_Key)
{
    switch (p_Key)
    {
    case GLFW_KEY_SPACE:
        return Key_Space;
    case GLFW_KEY_APOSTROPHE:
        return Key_Apostrophe;
    case GLFW_KEY_COMMA:
        return Key_Comma;
    case GLFW_KEY_MINUS:
        return Key_Minus;
    case GLFW_KEY_PERIOD:
        return Key_Period;
    case GLFW_KEY_SLASH:
        return Key_Slash;
    case GLFW_KEY_0:
        return Key_N0;
    case GLFW_KEY_1:
        return Key_N1;
    case GLFW_KEY_2:
        return Key_N2;
    case GLFW_KEY_3:
        return Key_N3;
    case GLFW_KEY_4:
        return Key_N4;
    case GLFW_KEY_5:
        return Key_N5;
    case GLFW_KEY_6:
        return Key_N6;
    case GLFW_KEY_7:
        return Key_N7;
    case GLFW_KEY_8:
        return Key_N8;
    case GLFW_KEY_9:
        return Key_N9;
    case GLFW_KEY_SEMICOLON:
        return Key_Semicolon;
    case GLFW_KEY_EQUAL:
        return Key_Equal;
    case GLFW_KEY_A:
        return Key_A;
    case GLFW_KEY_B:
        return Key_B;
    case GLFW_KEY_C:
        return Key_C;
    case GLFW_KEY_D:
        return Key_D;
    case GLFW_KEY_E:
        return Key_E;
    case GLFW_KEY_F:
        return Key_F;
    case GLFW_KEY_G:
        return Key_G;
    case GLFW_KEY_H:
        return Key_H;
    case GLFW_KEY_I:
        return Key_I;
    case GLFW_KEY_J:
        return Key_J;
    case GLFW_KEY_K:
        return Key_K;
    case GLFW_KEY_L:
        return Key_L;
    case GLFW_KEY_M:
        return Key_M;
    case GLFW_KEY_N:
        return Key_N;
    case GLFW_KEY_O:
        return Key_O;
    case GLFW_KEY_P:
        return Key_P;
    case GLFW_KEY_Q:
        return Key_Q;
    case GLFW_KEY_R:
        return Key_R;
    case GLFW_KEY_S:
        return Key_S;
    case GLFW_KEY_T:
        return Key_T;
    case GLFW_KEY_U:
        return Key_U;
    case GLFW_KEY_V:
        return Key_V;
    case GLFW_KEY_W:
        return Key_W;
    case GLFW_KEY_X:
        return Key_X;
    case GLFW_KEY_Y:
        return Key_Y;
    case GLFW_KEY_Z:
        return Key_Z;
    case GLFW_KEY_LEFT_BRACKET:
        return Key_LeftBracket;
    case GLFW_KEY_BACKSLASH:
        return Key_Backslash;
    case GLFW_KEY_RIGHT_BRACKET:
        return Key_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT:
        return Key_GraveAccent;
    case GLFW_KEY_WORLD_1:
        return Key_World_1;
    case GLFW_KEY_WORLD_2:
        return Key_World_2;
    case GLFW_KEY_ESCAPE:
        return Key_Escape;
    case GLFW_KEY_ENTER:
        return Key_Enter;
    case GLFW_KEY_TAB:
        return Key_Tab;
    case GLFW_KEY_BACKSPACE:
        return Key_Backspace;
    case GLFW_KEY_INSERT:
        return Key_Insert;
    case GLFW_KEY_DELETE:
        return Key_Delete;
    case GLFW_KEY_RIGHT:
        return Key_Right;
    case GLFW_KEY_LEFT:
        return Key_Left;
    case GLFW_KEY_DOWN:
        return Key_Down;
    case GLFW_KEY_UP:
        return Key_Up;
    case GLFW_KEY_PAGE_UP:
        return Key_PageUp;
    case GLFW_KEY_PAGE_DOWN:
        return Key_PageDown;
    case GLFW_KEY_HOME:
        return Key_Home;
    case GLFW_KEY_END:
        return Key_End;
    case GLFW_KEY_CAPS_LOCK:
        return Key_CapsLock;
    case GLFW_KEY_SCROLL_LOCK:
        return Key_ScrollLock;
    case GLFW_KEY_NUM_LOCK:
        return Key_NumLock;
    case GLFW_KEY_PRINT_SCREEN:
        return Key_PrintScreen;
    case GLFW_KEY_PAUSE:
        return Key_Pause;
    case GLFW_KEY_F1:
        return Key_F1;
    case GLFW_KEY_F2:
        return Key_F2;
    case GLFW_KEY_F3:
        return Key_F3;
    case GLFW_KEY_F4:
        return Key_F4;
    case GLFW_KEY_F5:
        return Key_F5;
    case GLFW_KEY_F6:
        return Key_F6;
    case GLFW_KEY_F7:
        return Key_F7;
    case GLFW_KEY_F8:
        return Key_F8;
    case GLFW_KEY_F9:
        return Key_F9;
    case GLFW_KEY_F10:
        return Key_F10;
    case GLFW_KEY_F11:
        return Key_F11;
    case GLFW_KEY_F12:
        return Key_F12;
    case GLFW_KEY_F13:
        return Key_F13;
    case GLFW_KEY_F14:
        return Key_F14;
    case GLFW_KEY_F15:
        return Key_F15;
    case GLFW_KEY_F16:
        return Key_F16;
    case GLFW_KEY_F17:
        return Key_F17;
    case GLFW_KEY_F18:
        return Key_F18;
    case GLFW_KEY_F19:
        return Key_F19;
    case GLFW_KEY_F20:
        return Key_F20;
    case GLFW_KEY_F21:
        return Key_F21;
    case GLFW_KEY_F22:
        return Key_F22;
    case GLFW_KEY_F23:
        return Key_F23;
    case GLFW_KEY_F24:
        return Key_F24;
    case GLFW_KEY_F25:
        return Key_F25;
    case GLFW_KEY_KP_0:
        return Key_KP_0;
    case GLFW_KEY_KP_1:
        return Key_KP_1;
    case GLFW_KEY_KP_2:
        return Key_KP_2;
    case GLFW_KEY_KP_3:
        return Key_KP_3;
    case GLFW_KEY_KP_4:
        return Key_KP_4;
    case GLFW_KEY_KP_5:
        return Key_KP_5;
    case GLFW_KEY_KP_6:
        return Key_KP_6;
    case GLFW_KEY_KP_7:
        return Key_KP_7;
    case GLFW_KEY_KP_8:
        return Key_KP_8;
    case GLFW_KEY_KP_9:
        return Key_KP_9;
    case GLFW_KEY_KP_DECIMAL:
        return Key_KPDecimal;
    case GLFW_KEY_KP_DIVIDE:
        return Key_KPDivide;
    case GLFW_KEY_KP_MULTIPLY:
        return Key_KPMultiply;
    case GLFW_KEY_KP_SUBTRACT:
        return Key_KPSubtract;
    case GLFW_KEY_KP_ADD:
        return Key_KPAdd;
    case GLFW_KEY_KP_ENTER:
        return Key_KPEnter;
    case GLFW_KEY_KP_EQUAL:
        return Key_KPEqual;
    case GLFW_KEY_LEFT_SHIFT:
        return Key_LeftShift;
    case GLFW_KEY_LEFT_CONTROL:
        return Key_LeftControl;
    case GLFW_KEY_LEFT_ALT:
        return Key_LeftAlt;
    case GLFW_KEY_LEFT_SUPER:
        return Key_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT:
        return Key_RightShift;
    case GLFW_KEY_RIGHT_CONTROL:
        return Key_RightControl;
    case GLFW_KEY_RIGHT_ALT:
        return Key_RightAlt;
    case GLFW_KEY_RIGHT_SUPER:
        return Key_RightSuper;
    case GLFW_KEY_MENU:
        return Key_Menu;
    default:
        return Key_None;
    }
}

static Mouse toMouse(const i32 p_Mouse)
{
    switch (p_Mouse)
    {
    case GLFW_MOUSE_BUTTON_1:
        return Mouse_Button1;
    case GLFW_MOUSE_BUTTON_2:
        return Mouse_Button2;
    case GLFW_MOUSE_BUTTON_3:
        return Mouse_Button3;
    case GLFW_MOUSE_BUTTON_4:
        return Mouse_Button4;
    case GLFW_MOUSE_BUTTON_5:
        return Mouse_Button5;
    case GLFW_MOUSE_BUTTON_6:
        return Mouse_Button6;
    case GLFW_MOUSE_BUTTON_7:
        return Mouse_Button7;
    case GLFW_MOUSE_BUTTON_8:
        return Mouse_Button8;
        // case GLFW_MOUSE_BUTTON_LAST:
        //     return Mouse_ButtonLast;
        // case GLFW_MOUSE_BUTTON_LEFT:
        //     return Mouse_ButtonLeft;
        // case GLFW_MOUSE_BUTTON_RIGHT:
        //     return Mouse_ButtonRight;
        // case GLFW_MOUSE_BUTTON_MIDDLE:
        // return Mouse_ButtonMiddle;
    default:
        return Mouse_None;
    }
}

void PollEvents()
{
    glfwPollEvents();
}
f32v2 GetScreenMousePosition(Window *p_Window)
{
    GLFWwindow *window = p_Window->GetWindowHandle();
    f64 xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);
    return f32v2{2.f * static_cast<f32>(xPos) / p_Window->GetScreenWidth() - 1.f,
                 1.f - 2.f * static_cast<f32>(yPos) / p_Window->GetScreenHeight()};
}

bool IsKeyPressed(Window *p_Window, const Key p_Key)
{
    return glfwGetKey(p_Window->GetWindowHandle(), toGlfw(p_Key)) == GLFW_PRESS;
}
bool IsKeyReleased(Window *p_Window, const Key p_Key)
{
    return glfwGetKey(p_Window->GetWindowHandle(), toGlfw(p_Key)) == GLFW_RELEASE;
}

bool IsMouseButtonPressed(Window *p_Window, const Mouse p_Button)
{
    return glfwGetMouseButton(p_Window->GetWindowHandle(), toGlfw(p_Button)) == GLFW_PRESS;
}
bool IsMouseButtonReleased(Window *p_Window, const Mouse p_Button)
{
    return glfwGetMouseButton(p_Window->GetWindowHandle(), toGlfw(p_Button)) == GLFW_RELEASE;
}

const char *GetKeyName(const Key p_Key)
{
    switch (p_Key)
    {
    case Key_Space:
        return "Space";
    case Key_Apostrophe:
        return "Apostrophe";
    case Key_Comma:
        return "Comma";
    case Key_Minus:
        return "Minus";
    case Key_Period:
        return "Period";
    case Key_Slash:
        return "Slash";
    case Key_N0:
        return "N0";
    case Key_N1:
        return "N1";
    case Key_N2:
        return "N2";
    case Key_N3:
        return "N3";
    case Key_N4:
        return "N4";
    case Key_N5:
        return "N5";
    case Key_N6:
        return "N6";
    case Key_N7:
        return "N7";
    case Key_N8:
        return "N8";
    case Key_N9:
        return "N9";
    case Key_Semicolon:
        return "Semicolon";
    case Key_Equal:
        return "Equal";
    case Key_A:
        return "A";
    case Key_B:
        return "B";
    case Key_C:
        return "C";
    case Key_D:
        return "D";
    case Key_E:
        return "E";
    case Key_F:
        return "F";
    case Key_G:
        return "G";
    case Key_H:
        return "H";
    case Key_I:
        return "I";
    case Key_J:
        return "J";
    case Key_K:
        return "K";
    case Key_L:
        return "L";
    case Key_M:
        return "M";
    case Key_N:
        return "N";
    case Key_O:
        return "O";
    case Key_P:
        return "P";
    case Key_Q:
        return "Q";
    case Key_R:
        return "R";
    case Key_S:
        return "S";
    case Key_T:
        return "T";
    case Key_U:
        return "U";
    case Key_V:
        return "V";
    case Key_W:
        return "W";
    case Key_X:
        return "X";
    case Key_Y:
        return "Y";
    case Key_Z:
        return "Z";
    case Key_LeftBracket:
        return "LeftBracket";
    case Key_Backslash:
        return "Backslash";
    case Key_RightBracket:
        return "RightBracket";
    case Key_GraveAccent:
        return "GraveAccent";
    case Key_World_1:
        return "World_1";
    case Key_World_2:
        return "World_2";
    case Key_Escape:
        return "Escape";
    case Key_Enter:
        return "Enter";
    case Key_Tab:
        return "Tab";
    case Key_Backspace:
        return "Backspace";
    case Key_Insert:
        return "Insert";
    case Key_Delete:
        return "Delete";
    case Key_Right:
        return "Right";
    case Key_Left:
        return "Left";
    case Key_Down:
        return "Down";
    case Key_Up:
        return "Up";
    case Key_PageUp:
        return "PageUp";
    case Key_PageDown:
        return "PageDown";
    case Key_Home:
        return "Home";
    case Key_End:
        return "End";
    case Key_CapsLock:
        return "CapsLock";
    case Key_ScrollLock:
        return "ScrollLock";
    case Key_NumLock:
        return "NumLock";
    case Key_PrintScreen:
        return "PrintScreen";
    case Key_Pause:
        return "Pause";
    case Key_F1:
        return "F1";
    case Key_F2:
        return "F2";
    case Key_F3:
        return "F3";
    case Key_F4:
        return "F4";
    case Key_F5:
        return "F5";
    case Key_F6:
        return "F6";
    case Key_F7:
        return "F7";
    case Key_F8:
        return "F8";
    case Key_F9:
        return "F9";
    case Key_F10:
        return "F10";
    case Key_F11:
        return "F11";
    case Key_F12:
        return "F12";
    case Key_F13:
        return "F13";
    case Key_F14:
        return "F14";
    case Key_F15:
        return "F15";
    case Key_F16:
        return "F16";
    case Key_F17:
        return "F17";
    case Key_F18:
        return "F18";
    case Key_F19:
        return "F19";
    case Key_F20:
        return "F20";
    case Key_F21:
        return "F21";
    case Key_F22:
        return "F22";
    case Key_F23:
        return "F23";
    case Key_F24:
        return "F24";
    case Key_F25:
        return "F25";
    case Key_KP_0:
        return "KP_0";
    case Key_KP_1:
        return "KP_1";
    case Key_KP_2:
        return "KP_2";
    case Key_KP_3:
        return "KP_3";
    case Key_KP_4:
        return "KP_4";
    case Key_KP_5:
        return "KP_5";
    case Key_KP_6:
        return "KP_6";
    case Key_KP_7:
        return "KP_7";
    case Key_KP_8:
        return "KP_8";
    case Key_KP_9:
        return "KP_9";
    case Key_KPDecimal:
        return "KPDecimal";
    case Key_KPDivide:
        return "KPDivide";
    case Key_KPMultiply:
        return "KPMultiply";
    case Key_KPSubtract:
        return "KPSubtract";
    case Key_KPAdd:
        return "KPAdd";
    case Key_KPEnter:
        return "KPEnter";
    case Key_KPEqual:
        return "KPEqual";
    case Key_LeftShift:
        return "LeftShift";
    case Key_LeftControl:
        return "LeftControl";
    case Key_LeftAlt:
        return "LeftAlt";
    case Key_LeftSuper:
        return "LeftSuper";
    case Key_RightShift:
        return "RightShift";
    case Key_RightControl:
        return "RightControl";
    case Key_RightAlt:
        return "RightAlt";
    case Key_RightSuper:
        return "RightSuper";
    case Key_Menu:
        return "Menu";
    case Key_None:
        return "None";
    case Key_Count:
        return "Unknown";
    }
    return "Unknown";
}

static void windowMoveCallback(GLFWwindow *p_Window, const i32 p_X, const i32 p_Y)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_WindowMoved;

    event.WindowDelta.Old = window->GetScreenDimensions();
    event.WindowDelta.New = u32v2{static_cast<u32>(p_X), static_cast<u32>(p_Y)};

    window->PushEvent(event);
    window->UpdateMonitorDeltaTime(window->GetMonitorDeltaTime());
}
} // namespace Onyx::Input

namespace Onyx
{
using namespace Onyx::Input;
void windowResizeCallback(GLFWwindow *p_Window, const i32 p_Width, const i32 p_Height)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_WindowResized;

    event.WindowDelta.Old = window->GetScreenDimensions();
    event.WindowDelta.New = u32v2{static_cast<u32>(p_Width), static_cast<u32>(p_Height)};

    window->m_Dimensions = event.WindowDelta.New;
    window->RequestSwapchainRecreation();
    window->PushEvent(event);
}
} // namespace Onyx

namespace Onyx::Input
{

static void framebufferResizeCallback(GLFWwindow *p_Window, const i32 p_Width, const i32 p_Height)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_FramebufferResized;

    event.WindowDelta.New = u32v2{static_cast<u32>(p_Width), static_cast<u32>(p_Height)};
    window->PushEvent(event);
}

static void windowFocusCallback(GLFWwindow *p_Window, const i32 p_Focused)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = p_Focused ? Event_WindowFocused : Event_WindowUnfocused;
    window->PushEvent(event);
}

static void windowCloseCallback(GLFWwindow *p_Window)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_WindowClosed;
    window->PushEvent(event);
}

static void windowIconifyCallback(GLFWwindow *p_Window, const i32 p_Iconified)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = p_Iconified ? Event_WindowMinimized : Event_WindowRestored;
    window->PushEvent(event);
}

static void keyCallback(GLFWwindow *p_Window, const i32 p_Key, const i32, const i32 p_Action, const i32)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);

    switch (p_Action)
    {
    case GLFW_PRESS:
        event.Type = Event_KeyPressed;
        break;
    case GLFW_RELEASE:
        event.Type = Event_KeyReleased;
        break;
    case GLFW_REPEAT:
        event.Type = Event_KeyRepeat;
        break;
    default:
        return;
    }

    event.Key = toKey(p_Key);
    window->PushEvent(event);
}

static void charCallback(GLFWwindow *p_Window, const u32 p_Codepoint)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_CharInput;
    event.Character.Codepoint = p_Codepoint;
    window->PushEvent(event);
}

static void cursorPositionCallback(GLFWwindow *p_Window, const f64 p_XPos, const f64 p_YPos)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_MouseMoved;
    event.Mouse.Position = f32v2{static_cast<f32>(p_XPos), static_cast<f32>(p_YPos)};
    window->PushEvent(event);
}

static void cursorEnterCallback(GLFWwindow *p_Window, const i32 p_Entered)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = p_Entered ? Event_MouseEntered : Event_MouseLeft;
    window->PushEvent(event);
}

static void mouseButtonCallback(GLFWwindow *p_Window, const i32 p_Button, const i32 p_Action, const i32)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = p_Action == GLFW_PRESS ? Event_MousePressed : Event_MouseReleased;
    event.Mouse.Button = toMouse(p_Button);
    window->PushEvent(event);
}

static void scrollCallback(GLFWwindow *p_Window, const f64 p_XOffset, const f64 p_YOffset)
{
    Event event{};
    Window *window = windowFromGLFW(p_Window);
    event.Type = Event_Scrolled;
    event.ScrollOffset = f32v2{static_cast<f32>(p_XOffset), static_cast<f32>(p_YOffset)};
    window->PushEvent(event);
}

void InstallCallbacks(Window &p_Window)
{
    GLFWwindow *window = p_Window.GetWindowHandle();

    glfwSetWindowPosCallback(window, windowMoveCallback);
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetWindowFocusCallback(window, windowFocusCallback);
    glfwSetWindowCloseCallback(window, windowCloseCallback);
    glfwSetWindowIconifyCallback(window, windowIconifyCallback);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);

    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetCursorEnterCallback(window, cursorEnterCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
}
} // namespace Onyx::Input
