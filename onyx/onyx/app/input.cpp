#include "onyx/core/pch.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"

namespace Onyx::Input
{
static Window *windowFromGLFW(GLFWwindow *p_Window)
{
    return static_cast<Window *>(glfwGetWindowUserPointer(p_Window));
}

void PollEvents()
{
    glfwPollEvents();
}
fvec2 GetScreenMousePosition(Window *p_Window) noexcept
{
    GLFWwindow *window = p_Window->GetWindowHandle();
    double xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);
    return {2.f * static_cast<f32>(xPos) / p_Window->GetScreenWidth() - 1.f,
            1.f - 2.f * static_cast<f32>(yPos) / p_Window->GetScreenHeight()};
}

bool IsKeyPressed(Window *p_Window, const Key p_Key) noexcept
{
    return glfwGetKey(p_Window->GetWindowHandle(), static_cast<i32>(p_Key)) == GLFW_PRESS;
}
bool IsKeyReleased(Window *p_Window, const Key p_Key) noexcept
{
    return glfwGetKey(p_Window->GetWindowHandle(), static_cast<i32>(p_Key)) == GLFW_RELEASE;
}

bool IsMouseButtonPressed(Window *p_Window, const Mouse p_Button) noexcept
{
    return glfwGetMouseButton(p_Window->GetWindowHandle(), static_cast<i32>(p_Button)) == GLFW_PRESS;
}
bool IsMouseButtonReleased(Window *p_Window, const Mouse p_Button) noexcept
{
    return glfwGetMouseButton(p_Window->GetWindowHandle(), static_cast<i32>(p_Button)) == GLFW_RELEASE;
}

const char *GetKeyName(const Key p_Key) noexcept
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

static void windowResizeCallback(GLFWwindow *p_Window, const i32 p_Width, const i32 p_Height)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::WindowResized;

    Window *window = event.Window;

    event.WindowResize.OldWidth = window->GetScreenWidth();
    event.WindowResize.OldHeight = window->GetScreenHeight();

    event.WindowResize.NewWidth = static_cast<u32>(p_Width);
    event.WindowResize.NewHeight = static_cast<u32>(p_Height);

    window->flagResize(event.WindowResize.NewWidth, event.WindowResize.NewHeight);
    window->PushEvent(event);
}

static void windowFocusCallback(GLFWwindow *p_Window, const i32 p_Focused)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Focused ? Event::WindowFocused : Event::WindowUnfocused;
    event.Window->PushEvent(event);
}

static void keyCallback(GLFWwindow *p_Window, const i32 p_Key, const i32, const i32 p_Action, const i32)
{
    Event event;
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
        break;
    }
    event.Key = static_cast<Key>(p_Key);
    event.Window->PushEvent(event);
}

static void cursorPositionCallback(GLFWwindow *p_Window, const double p_XPos, const double p_YPos)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::MouseMoved;
    event.Mouse.Position = {static_cast<f32>(p_XPos), static_cast<f32>(p_YPos)};
    event.Window->PushEvent(event);
}

static void cursorEnterCallback(GLFWwindow *p_Window, const i32 p_Entered)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Entered ? Event::MouseEntered : Event::MouseLeft;
    event.Window->PushEvent(event);
}

static void mouseButtonCallback(GLFWwindow *p_Window, const i32 p_Button, const i32 p_Action, const i32)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = p_Action == GLFW_PRESS ? Event::MousePressed : Event::MouseReleased;
    event.Mouse.Button = static_cast<Mouse>(p_Button);
    event.Window->PushEvent(event);
}

static void scrollCallback(GLFWwindow *p_Window, double p_XOffset, double p_YOffset)
{
    Event event;
    event.Window = windowFromGLFW(p_Window);
    event.Type = Event::Scrolled;
    event.ScrollOffset = {static_cast<f32>(p_XOffset), static_cast<f32>(p_YOffset)};
    event.Window->PushEvent(event);
}

void InstallCallbacks(Window &p_Window) noexcept
{
    glfwSetWindowSizeCallback(p_Window.GetWindowHandle(), windowResizeCallback);
    glfwSetWindowFocusCallback(p_Window.GetWindowHandle(), windowFocusCallback);
    glfwSetKeyCallback(p_Window.GetWindowHandle(), keyCallback);
    glfwSetCursorPosCallback(p_Window.GetWindowHandle(), cursorPositionCallback);
    glfwSetCursorEnterCallback(p_Window.GetWindowHandle(), cursorEnterCallback);
    glfwSetMouseButtonCallback(p_Window.GetWindowHandle(), mouseButtonCallback);
    glfwSetScrollCallback(p_Window.GetWindowHandle(), scrollCallback);
}

} // namespace Onyx::Input