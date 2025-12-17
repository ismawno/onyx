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
    case Key::Space:
        return GLFW_KEY_SPACE;
    case Key::Apostrophe:
        return GLFW_KEY_APOSTROPHE;
    case Key::Comma:
        return GLFW_KEY_COMMA;
    case Key::Minus:
        return GLFW_KEY_MINUS;
    case Key::Period:
        return GLFW_KEY_PERIOD;
    case Key::Slash:
        return GLFW_KEY_SLASH;
    case Key::N0:
        return GLFW_KEY_0;
    case Key::N1:
        return GLFW_KEY_1;
    case Key::N2:
        return GLFW_KEY_2;
    case Key::N3:
        return GLFW_KEY_3;
    case Key::N4:
        return GLFW_KEY_4;
    case Key::N5:
        return GLFW_KEY_5;
    case Key::N6:
        return GLFW_KEY_6;
    case Key::N7:
        return GLFW_KEY_7;
    case Key::N8:
        return GLFW_KEY_8;
    case Key::N9:
        return GLFW_KEY_9;
    case Key::Semicolon:
        return GLFW_KEY_SEMICOLON;
    case Key::Equal:
        return GLFW_KEY_EQUAL;
    case Key::A:
        return GLFW_KEY_A;
    case Key::B:
        return GLFW_KEY_B;
    case Key::C:
        return GLFW_KEY_C;
    case Key::D:
        return GLFW_KEY_D;
    case Key::E:
        return GLFW_KEY_E;
    case Key::F:
        return GLFW_KEY_F;
    case Key::G:
        return GLFW_KEY_G;
    case Key::H:
        return GLFW_KEY_H;
    case Key::I:
        return GLFW_KEY_I;
    case Key::J:
        return GLFW_KEY_J;
    case Key::K:
        return GLFW_KEY_K;
    case Key::L:
        return GLFW_KEY_L;
    case Key::M:
        return GLFW_KEY_M;
    case Key::N:
        return GLFW_KEY_N;
    case Key::O:
        return GLFW_KEY_O;
    case Key::P:
        return GLFW_KEY_P;
    case Key::Q:
        return GLFW_KEY_Q;
    case Key::R:
        return GLFW_KEY_R;
    case Key::S:
        return GLFW_KEY_S;
    case Key::T:
        return GLFW_KEY_T;
    case Key::U:
        return GLFW_KEY_U;
    case Key::V:
        return GLFW_KEY_V;
    case Key::W:
        return GLFW_KEY_W;
    case Key::X:
        return GLFW_KEY_X;
    case Key::Y:
        return GLFW_KEY_Y;
    case Key::Z:
        return GLFW_KEY_Z;
    case Key::LeftBracket:
        return GLFW_KEY_LEFT_BRACKET;
    case Key::Backslash:
        return GLFW_KEY_BACKSLASH;
    case Key::RightBracket:
        return GLFW_KEY_RIGHT_BRACKET;
    case Key::GraveAccent:
        return GLFW_KEY_GRAVE_ACCENT;
    case Key::World_1:
        return GLFW_KEY_WORLD_1;
    case Key::World_2:
        return GLFW_KEY_WORLD_2;
    case Key::Escape:
        return GLFW_KEY_ESCAPE;
    case Key::Enter:
        return GLFW_KEY_ENTER;
    case Key::Tab:
        return GLFW_KEY_TAB;
    case Key::Backspace:
        return GLFW_KEY_BACKSPACE;
    case Key::Insert:
        return GLFW_KEY_INSERT;
    case Key::Delete:
        return GLFW_KEY_DELETE;
    case Key::Right:
        return GLFW_KEY_RIGHT;
    case Key::Left:
        return GLFW_KEY_LEFT;
    case Key::Down:
        return GLFW_KEY_DOWN;
    case Key::Up:
        return GLFW_KEY_UP;
    case Key::PageUp:
        return GLFW_KEY_PAGE_UP;
    case Key::PageDown:
        return GLFW_KEY_PAGE_DOWN;
    case Key::Home:
        return GLFW_KEY_HOME;
    case Key::End:
        return GLFW_KEY_END;
    case Key::CapsLock:
        return GLFW_KEY_CAPS_LOCK;
    case Key::ScrollLock:
        return GLFW_KEY_SCROLL_LOCK;
    case Key::NumLock:
        return GLFW_KEY_NUM_LOCK;
    case Key::PrintScreen:
        return GLFW_KEY_PRINT_SCREEN;
    case Key::Pause:
        return GLFW_KEY_PAUSE;
    case Key::F1:
        return GLFW_KEY_F1;
    case Key::F2:
        return GLFW_KEY_F2;
    case Key::F3:
        return GLFW_KEY_F3;
    case Key::F4:
        return GLFW_KEY_F4;
    case Key::F5:
        return GLFW_KEY_F5;
    case Key::F6:
        return GLFW_KEY_F6;
    case Key::F7:
        return GLFW_KEY_F7;
    case Key::F8:
        return GLFW_KEY_F8;
    case Key::F9:
        return GLFW_KEY_F9;
    case Key::F10:
        return GLFW_KEY_F10;
    case Key::F11:
        return GLFW_KEY_F11;
    case Key::F12:
        return GLFW_KEY_F12;
    case Key::F13:
        return GLFW_KEY_F13;
    case Key::F14:
        return GLFW_KEY_F14;
    case Key::F15:
        return GLFW_KEY_F15;
    case Key::F16:
        return GLFW_KEY_F16;
    case Key::F17:
        return GLFW_KEY_F17;
    case Key::F18:
        return GLFW_KEY_F18;
    case Key::F19:
        return GLFW_KEY_F19;
    case Key::F20:
        return GLFW_KEY_F20;
    case Key::F21:
        return GLFW_KEY_F21;
    case Key::F22:
        return GLFW_KEY_F22;
    case Key::F23:
        return GLFW_KEY_F23;
    case Key::F24:
        return GLFW_KEY_F24;
    case Key::F25:
        return GLFW_KEY_F25;
    case Key::KP_0:
        return GLFW_KEY_KP_0;
    case Key::KP_1:
        return GLFW_KEY_KP_1;
    case Key::KP_2:
        return GLFW_KEY_KP_2;
    case Key::KP_3:
        return GLFW_KEY_KP_3;
    case Key::KP_4:
        return GLFW_KEY_KP_4;
    case Key::KP_5:
        return GLFW_KEY_KP_5;
    case Key::KP_6:
        return GLFW_KEY_KP_6;
    case Key::KP_7:
        return GLFW_KEY_KP_7;
    case Key::KP_8:
        return GLFW_KEY_KP_8;
    case Key::KP_9:
        return GLFW_KEY_KP_9;
    case Key::KPDecimal:
        return GLFW_KEY_KP_DECIMAL;
    case Key::KPDivide:
        return GLFW_KEY_KP_DIVIDE;
    case Key::KPMultiply:
        return GLFW_KEY_KP_MULTIPLY;
    case Key::KPSubtract:
        return GLFW_KEY_KP_SUBTRACT;
    case Key::KPAdd:
        return GLFW_KEY_KP_ADD;
    case Key::KPEnter:
        return GLFW_KEY_KP_ENTER;
    case Key::KPEqual:
        return GLFW_KEY_KP_EQUAL;
    case Key::LeftShift:
        return GLFW_KEY_LEFT_SHIFT;
    case Key::LeftControl:
        return GLFW_KEY_LEFT_CONTROL;
    case Key::LeftAlt:
        return GLFW_KEY_LEFT_ALT;
    case Key::LeftSuper:
        return GLFW_KEY_LEFT_SUPER;
    case Key::RightShift:
        return GLFW_KEY_RIGHT_SHIFT;
    case Key::RightControl:
        return GLFW_KEY_RIGHT_CONTROL;
    case Key::RightAlt:
        return GLFW_KEY_RIGHT_ALT;
    case Key::RightSuper:
        return GLFW_KEY_RIGHT_SUPER;
    case Key::Menu:
        return GLFW_KEY_MENU;
    case Key::None:
        return GLFW_KEY_LAST + 1;
    }
    return GLFW_KEY_LAST + 1;
}

static i32 toGlfw(const Mouse p_Mouse)
{
    switch (p_Mouse)
    {
    case Mouse::Button1:
        return GLFW_MOUSE_BUTTON_1;
    case Mouse::Button2:
        return GLFW_MOUSE_BUTTON_2;
    case Mouse::Button3:
        return GLFW_MOUSE_BUTTON_3;
    case Mouse::Button4:
        return GLFW_MOUSE_BUTTON_4;
    case Mouse::Button5:
        return GLFW_MOUSE_BUTTON_5;
    case Mouse::Button6:
        return GLFW_MOUSE_BUTTON_6;
    case Mouse::Button7:
        return GLFW_MOUSE_BUTTON_7;
    case Mouse::Button8:
        return GLFW_MOUSE_BUTTON_8;
    case Mouse::ButtonLast:
        return GLFW_MOUSE_BUTTON_LAST;
    case Mouse::ButtonLeft:
        return GLFW_MOUSE_BUTTON_LEFT;
    case Mouse::ButtonRight:
        return GLFW_MOUSE_BUTTON_RIGHT;
    case Mouse::ButtonMiddle:
        return GLFW_MOUSE_BUTTON_MIDDLE;
    case Mouse::None:
        return GLFW_MOUSE_BUTTON_LAST + 1;
    }
    return GLFW_MOUSE_BUTTON_LAST + 1;
}

static Key toKey(const i32 p_Key)
{
    switch (p_Key)
    {
    case GLFW_KEY_SPACE:
        return Key::Space;
    case GLFW_KEY_APOSTROPHE:
        return Key::Apostrophe;
    case GLFW_KEY_COMMA:
        return Key::Comma;
    case GLFW_KEY_MINUS:
        return Key::Minus;
    case GLFW_KEY_PERIOD:
        return Key::Period;
    case GLFW_KEY_SLASH:
        return Key::Slash;
    case GLFW_KEY_0:
        return Key::N0;
    case GLFW_KEY_1:
        return Key::N1;
    case GLFW_KEY_2:
        return Key::N2;
    case GLFW_KEY_3:
        return Key::N3;
    case GLFW_KEY_4:
        return Key::N4;
    case GLFW_KEY_5:
        return Key::N5;
    case GLFW_KEY_6:
        return Key::N6;
    case GLFW_KEY_7:
        return Key::N7;
    case GLFW_KEY_8:
        return Key::N8;
    case GLFW_KEY_9:
        return Key::N9;
    case GLFW_KEY_SEMICOLON:
        return Key::Semicolon;
    case GLFW_KEY_EQUAL:
        return Key::Equal;
    case GLFW_KEY_A:
        return Key::A;
    case GLFW_KEY_B:
        return Key::B;
    case GLFW_KEY_C:
        return Key::C;
    case GLFW_KEY_D:
        return Key::D;
    case GLFW_KEY_E:
        return Key::E;
    case GLFW_KEY_F:
        return Key::F;
    case GLFW_KEY_G:
        return Key::G;
    case GLFW_KEY_H:
        return Key::H;
    case GLFW_KEY_I:
        return Key::I;
    case GLFW_KEY_J:
        return Key::J;
    case GLFW_KEY_K:
        return Key::K;
    case GLFW_KEY_L:
        return Key::L;
    case GLFW_KEY_M:
        return Key::M;
    case GLFW_KEY_N:
        return Key::N;
    case GLFW_KEY_O:
        return Key::O;
    case GLFW_KEY_P:
        return Key::P;
    case GLFW_KEY_Q:
        return Key::Q;
    case GLFW_KEY_R:
        return Key::R;
    case GLFW_KEY_S:
        return Key::S;
    case GLFW_KEY_T:
        return Key::T;
    case GLFW_KEY_U:
        return Key::U;
    case GLFW_KEY_V:
        return Key::V;
    case GLFW_KEY_W:
        return Key::W;
    case GLFW_KEY_X:
        return Key::X;
    case GLFW_KEY_Y:
        return Key::Y;
    case GLFW_KEY_Z:
        return Key::Z;
    case GLFW_KEY_LEFT_BRACKET:
        return Key::LeftBracket;
    case GLFW_KEY_BACKSLASH:
        return Key::Backslash;
    case GLFW_KEY_RIGHT_BRACKET:
        return Key::RightBracket;
    case GLFW_KEY_GRAVE_ACCENT:
        return Key::GraveAccent;
    case GLFW_KEY_WORLD_1:
        return Key::World_1;
    case GLFW_KEY_WORLD_2:
        return Key::World_2;
    case GLFW_KEY_ESCAPE:
        return Key::Escape;
    case GLFW_KEY_ENTER:
        return Key::Enter;
    case GLFW_KEY_TAB:
        return Key::Tab;
    case GLFW_KEY_BACKSPACE:
        return Key::Backspace;
    case GLFW_KEY_INSERT:
        return Key::Insert;
    case GLFW_KEY_DELETE:
        return Key::Delete;
    case GLFW_KEY_RIGHT:
        return Key::Right;
    case GLFW_KEY_LEFT:
        return Key::Left;
    case GLFW_KEY_DOWN:
        return Key::Down;
    case GLFW_KEY_UP:
        return Key::Up;
    case GLFW_KEY_PAGE_UP:
        return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN:
        return Key::PageDown;
    case GLFW_KEY_HOME:
        return Key::Home;
    case GLFW_KEY_END:
        return Key::End;
    case GLFW_KEY_CAPS_LOCK:
        return Key::CapsLock;
    case GLFW_KEY_SCROLL_LOCK:
        return Key::ScrollLock;
    case GLFW_KEY_NUM_LOCK:
        return Key::NumLock;
    case GLFW_KEY_PRINT_SCREEN:
        return Key::PrintScreen;
    case GLFW_KEY_PAUSE:
        return Key::Pause;
    case GLFW_KEY_F1:
        return Key::F1;
    case GLFW_KEY_F2:
        return Key::F2;
    case GLFW_KEY_F3:
        return Key::F3;
    case GLFW_KEY_F4:
        return Key::F4;
    case GLFW_KEY_F5:
        return Key::F5;
    case GLFW_KEY_F6:
        return Key::F6;
    case GLFW_KEY_F7:
        return Key::F7;
    case GLFW_KEY_F8:
        return Key::F8;
    case GLFW_KEY_F9:
        return Key::F9;
    case GLFW_KEY_F10:
        return Key::F10;
    case GLFW_KEY_F11:
        return Key::F11;
    case GLFW_KEY_F12:
        return Key::F12;
    case GLFW_KEY_F13:
        return Key::F13;
    case GLFW_KEY_F14:
        return Key::F14;
    case GLFW_KEY_F15:
        return Key::F15;
    case GLFW_KEY_F16:
        return Key::F16;
    case GLFW_KEY_F17:
        return Key::F17;
    case GLFW_KEY_F18:
        return Key::F18;
    case GLFW_KEY_F19:
        return Key::F19;
    case GLFW_KEY_F20:
        return Key::F20;
    case GLFW_KEY_F21:
        return Key::F21;
    case GLFW_KEY_F22:
        return Key::F22;
    case GLFW_KEY_F23:
        return Key::F23;
    case GLFW_KEY_F24:
        return Key::F24;
    case GLFW_KEY_F25:
        return Key::F25;
    case GLFW_KEY_KP_0:
        return Key::KP_0;
    case GLFW_KEY_KP_1:
        return Key::KP_1;
    case GLFW_KEY_KP_2:
        return Key::KP_2;
    case GLFW_KEY_KP_3:
        return Key::KP_3;
    case GLFW_KEY_KP_4:
        return Key::KP_4;
    case GLFW_KEY_KP_5:
        return Key::KP_5;
    case GLFW_KEY_KP_6:
        return Key::KP_6;
    case GLFW_KEY_KP_7:
        return Key::KP_7;
    case GLFW_KEY_KP_8:
        return Key::KP_8;
    case GLFW_KEY_KP_9:
        return Key::KP_9;
    case GLFW_KEY_KP_DECIMAL:
        return Key::KPDecimal;
    case GLFW_KEY_KP_DIVIDE:
        return Key::KPDivide;
    case GLFW_KEY_KP_MULTIPLY:
        return Key::KPMultiply;
    case GLFW_KEY_KP_SUBTRACT:
        return Key::KPSubtract;
    case GLFW_KEY_KP_ADD:
        return Key::KPAdd;
    case GLFW_KEY_KP_ENTER:
        return Key::KPEnter;
    case GLFW_KEY_KP_EQUAL:
        return Key::KPEqual;
    case GLFW_KEY_LEFT_SHIFT:
        return Key::LeftShift;
    case GLFW_KEY_LEFT_CONTROL:
        return Key::LeftControl;
    case GLFW_KEY_LEFT_ALT:
        return Key::LeftAlt;
    case GLFW_KEY_LEFT_SUPER:
        return Key::LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT:
        return Key::RightShift;
    case GLFW_KEY_RIGHT_CONTROL:
        return Key::RightControl;
    case GLFW_KEY_RIGHT_ALT:
        return Key::RightAlt;
    case GLFW_KEY_RIGHT_SUPER:
        return Key::RightSuper;
    case GLFW_KEY_MENU:
        return Key::Menu;
    default:
        return Key::None;
    }
}

static Mouse toMouse(const i32 p_Mouse)
{
    switch (p_Mouse)
    {
    case GLFW_MOUSE_BUTTON_1:
        return Mouse::Button1;
    case GLFW_MOUSE_BUTTON_2:
        return Mouse::Button2;
    case GLFW_MOUSE_BUTTON_3:
        return Mouse::Button3;
    case GLFW_MOUSE_BUTTON_4:
        return Mouse::Button4;
    case GLFW_MOUSE_BUTTON_5:
        return Mouse::Button5;
    case GLFW_MOUSE_BUTTON_6:
        return Mouse::Button6;
    case GLFW_MOUSE_BUTTON_7:
        return Mouse::Button7;
    case GLFW_MOUSE_BUTTON_8:
        return Mouse::Button8;
        // case GLFW_MOUSE_BUTTON_LAST:
        //     return Mouse::ButtonLast;
        // case GLFW_MOUSE_BUTTON_LEFT:
        //     return Mouse::ButtonLeft;
        // case GLFW_MOUSE_BUTTON_RIGHT:
        //     return Mouse::ButtonRight;
        // case GLFW_MOUSE_BUTTON_MIDDLE:
        // return Mouse::ButtonMiddle;
    default:
        return Mouse::None;
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
    case Key::Space:
        return "Space";
    case Key::Apostrophe:
        return "Apostrophe";
    case Key::Comma:
        return "Comma";
    case Key::Minus:
        return "Minus";
    case Key::Period:
        return "Period";
    case Key::Slash:
        return "Slash";
    case Key::N0:
        return "N0";
    case Key::N1:
        return "N1";
    case Key::N2:
        return "N2";
    case Key::N3:
        return "N3";
    case Key::N4:
        return "N4";
    case Key::N5:
        return "N5";
    case Key::N6:
        return "N6";
    case Key::N7:
        return "N7";
    case Key::N8:
        return "N8";
    case Key::N9:
        return "N9";
    case Key::Semicolon:
        return "Semicolon";
    case Key::Equal:
        return "Equal";
    case Key::A:
        return "A";
    case Key::B:
        return "B";
    case Key::C:
        return "C";
    case Key::D:
        return "D";
    case Key::E:
        return "E";
    case Key::F:
        return "F";
    case Key::G:
        return "G";
    case Key::H:
        return "H";
    case Key::I:
        return "I";
    case Key::J:
        return "J";
    case Key::K:
        return "K";
    case Key::L:
        return "L";
    case Key::M:
        return "M";
    case Key::N:
        return "N";
    case Key::O:
        return "O";
    case Key::P:
        return "P";
    case Key::Q:
        return "Q";
    case Key::R:
        return "R";
    case Key::S:
        return "S";
    case Key::T:
        return "T";
    case Key::U:
        return "U";
    case Key::V:
        return "V";
    case Key::W:
        return "W";
    case Key::X:
        return "X";
    case Key::Y:
        return "Y";
    case Key::Z:
        return "Z";
    case Key::LeftBracket:
        return "LeftBracket";
    case Key::Backslash:
        return "Backslash";
    case Key::RightBracket:
        return "RightBracket";
    case Key::GraveAccent:
        return "GraveAccent";
    case Key::World_1:
        return "World_1";
    case Key::World_2:
        return "World_2";
    case Key::Escape:
        return "Escape";
    case Key::Enter:
        return "Enter";
    case Key::Tab:
        return "Tab";
    case Key::Backspace:
        return "Backspace";
    case Key::Insert:
        return "Insert";
    case Key::Delete:
        return "Delete";
    case Key::Right:
        return "Right";
    case Key::Left:
        return "Left";
    case Key::Down:
        return "Down";
    case Key::Up:
        return "Up";
    case Key::PageUp:
        return "PageUp";
    case Key::PageDown:
        return "PageDown";
    case Key::Home:
        return "Home";
    case Key::End:
        return "End";
    case Key::CapsLock:
        return "CapsLock";
    case Key::ScrollLock:
        return "ScrollLock";
    case Key::NumLock:
        return "NumLock";
    case Key::PrintScreen:
        return "PrintScreen";
    case Key::Pause:
        return "Pause";
    case Key::F1:
        return "F1";
    case Key::F2:
        return "F2";
    case Key::F3:
        return "F3";
    case Key::F4:
        return "F4";
    case Key::F5:
        return "F5";
    case Key::F6:
        return "F6";
    case Key::F7:
        return "F7";
    case Key::F8:
        return "F8";
    case Key::F9:
        return "F9";
    case Key::F10:
        return "F10";
    case Key::F11:
        return "F11";
    case Key::F12:
        return "F12";
    case Key::F13:
        return "F13";
    case Key::F14:
        return "F14";
    case Key::F15:
        return "F15";
    case Key::F16:
        return "F16";
    case Key::F17:
        return "F17";
    case Key::F18:
        return "F18";
    case Key::F19:
        return "F19";
    case Key::F20:
        return "F20";
    case Key::F21:
        return "F21";
    case Key::F22:
        return "F22";
    case Key::F23:
        return "F23";
    case Key::F24:
        return "F24";
    case Key::F25:
        return "F25";
    case Key::KP_0:
        return "KP_0";
    case Key::KP_1:
        return "KP_1";
    case Key::KP_2:
        return "KP_2";
    case Key::KP_3:
        return "KP_3";
    case Key::KP_4:
        return "KP_4";
    case Key::KP_5:
        return "KP_5";
    case Key::KP_6:
        return "KP_6";
    case Key::KP_7:
        return "KP_7";
    case Key::KP_8:
        return "KP_8";
    case Key::KP_9:
        return "KP_9";
    case Key::KPDecimal:
        return "KPDecimal";
    case Key::KPDivide:
        return "KPDivide";
    case Key::KPMultiply:
        return "KPMultiply";
    case Key::KPSubtract:
        return "KPSubtract";
    case Key::KPAdd:
        return "KPAdd";
    case Key::KPEnter:
        return "KPEnter";
    case Key::KPEqual:
        return "KPEqual";
    case Key::LeftShift:
        return "LeftShift";
    case Key::LeftControl:
        return "LeftControl";
    case Key::LeftAlt:
        return "LeftAlt";
    case Key::LeftSuper:
        return "LeftSuper";
    case Key::RightShift:
        return "RightShift";
    case Key::RightControl:
        return "RightControl";
    case Key::RightAlt:
        return "RightAlt";
    case Key::RightSuper:
        return "RightSuper";
    case Key::Menu:
        return "Menu";
    case Key::None:
        return "None";
    }
    return "Unknown";
}

static void windowMoveCallback(GLFWwindow *p_Window, const i32 p_X, const i32 p_Y)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::WindowMoved;

    Window *window = event.Window;

    event.WindowDelta.Old = window->GetScreenDimensions();
    event.WindowDelta.New = u32v2{static_cast<u32>(p_X), static_cast<u32>(p_Y)};

    window->PushEvent(event);
}

static void windowResizeCallback(GLFWwindow *p_Window, const i32 p_Width, const i32 p_Height)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::WindowResized;

    Window *window = event.Window;

    event.WindowDelta.Old = window->GetScreenDimensions();
    event.WindowDelta.New = u32v2{static_cast<u32>(p_Width), static_cast<u32>(p_Height)};

    window->flagResize(event.WindowDelta.New);
    window->PushEvent(event);
}

static void framebufferResizeCallback(GLFWwindow *p_Window, const i32 p_Width, const i32 p_Height)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::FramebufferResized;

    event.WindowDelta.New = u32v2{static_cast<u32>(p_Width), static_cast<u32>(p_Height)};

    event.Window->PushEvent(event);
}

static void windowFocusCallback(GLFWwindow *p_Window, const i32 p_Focused)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Focused ? Event::WindowFocused : Event::WindowUnfocused;
    event.Window->PushEvent(event);
}

static void windowCloseCallback(GLFWwindow *p_Window)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::WindowClosed;
    event.Window->PushEvent(event);
}

static void windowIconifyCallback(GLFWwindow *p_Window, const i32 p_Iconified)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Iconified ? Event::WindowMinimized : Event::WindowRestored;
    event.Window->PushEvent(event);
}

static void keyCallback(GLFWwindow *p_Window, const i32 p_Key, const i32, const i32 p_Action, const i32)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);

    switch (p_Action)
    {
    case GLFW_PRESS:
        event.Type = Event::KeyPressed;
        break;
    case GLFW_RELEASE:
        event.Type = Event::KeyReleased;
        break;
    case GLFW_REPEAT:
        event.Type = Event::KeyRepeat;
        break;
    default:
        return;
    }

    event.Key = toKey(p_Key);
    event.Window->PushEvent(event);
}

static void charCallback(GLFWwindow *p_Window, const u32 p_Codepoint)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::CharInput;
    event.Character.Codepoint = p_Codepoint;
    event.Window->PushEvent(event);
}

static void cursorPositionCallback(GLFWwindow *p_Window, const f64 p_XPos, const f64 p_YPos)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::MouseMoved;
    event.Mouse.Position = f32v2{static_cast<f32>(p_XPos), static_cast<f32>(p_YPos)};
    event.Window->PushEvent(event);
}

static void cursorEnterCallback(GLFWwindow *p_Window, const i32 p_Entered)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Entered ? Event::MouseEntered : Event::MouseLeft;
    event.Window->PushEvent(event);
}

static void mouseButtonCallback(GLFWwindow *p_Window, const i32 p_Button, const i32 p_Action, const i32)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Action == GLFW_PRESS ? Event::MousePressed : Event::MouseReleased;
    event.Mouse.Button = toMouse(p_Button);
    event.Window->PushEvent(event);
}

static void scrollCallback(GLFWwindow *p_Window, const f64 p_XOffset, const f64 p_YOffset)
{
    Event event{};
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::Scrolled;
    event.ScrollOffset = f32v2{static_cast<f32>(p_XOffset), static_cast<f32>(p_YOffset)};
    event.Window->PushEvent(event);
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
