#include "core/pch.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static Window *windowFromGLFW(GLFWwindow *p_Window)
{
    return static_cast<Window *>(glfwGetWindowUserPointer(p_Window));
}

void Input::PollEvents()
{
    glfwPollEvents();
}

vec2 Input::GetMousePosition(Window *p_Window) noexcept
{
    GLFWwindow *window = p_Window->GetWindowHandle();
    double xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);
    return {2.f * static_cast<f32>(xPos) / p_Window->GetScreenWidth() - 1.f,
            2.f * static_cast<f32>(yPos) / p_Window->GetScreenHeight() - 1.f};
}

bool Input::IsKeyPressed(const Key p_Key) noexcept
{
    return glfwGetKey(glfwGetCurrentContext(), static_cast<int>(p_Key)) == GLFW_PRESS;
}
bool Input::IsKeyReleased(const Key p_Key) noexcept
{
    return glfwGetKey(glfwGetCurrentContext(), static_cast<int>(p_Key)) == GLFW_RELEASE;
}

bool Input::IsMouseButtonPressed(const Mouse p_Button) noexcept
{
    return glfwGetMouseButton(glfwGetCurrentContext(), static_cast<int>(p_Button)) == GLFW_PRESS;
}
bool Input::IsMouseButtonReleased(const Mouse p_Button) noexcept
{
    return glfwGetMouseButton(glfwGetCurrentContext(), static_cast<int>(p_Button)) == GLFW_RELEASE;
}

const char *Input::GetKeyName(const Key p_Key) noexcept
{
    return glfwGetKeyName(static_cast<int>(p_Key), 0);
}

static void windowResizeCallback(GLFWwindow *p_Window, const int p_Width, const int p_Height)
{
    Event event;
    event.Type = Event::WINDOW_RESIZED;

    Window *window = windowFromGLFW(p_Window);

    event.Window.OldWidth = window->GetScreenWidth();
    event.Window.OldHeight = window->GetScreenHeight();

    event.Window.NewWidth = static_cast<u32>(p_Width);
    event.Window.NewHeight = static_cast<u32>(p_Height);

    window->FlagResize(event.Window.NewWidth, event.Window.NewHeight);
    window->PushEvent(event);
}

static void windowCloseCallback(GLFWwindow *p_Window)
{
    Event event;
    event.Type = Event::WINDOW_CLOSED;
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void windowFocusCallback(GLFWwindow *p_Window, const int p_Focused)
{
    Event event;
    event.Type = p_Focused ? Event::WINDOW_FOCUSED : Event::WINDOW_UNFOCUSED;
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void keyCallback(GLFWwindow *p_Window, const int p_Key, const int, const int p_Action, const int)
{
    Event event;
    switch (p_Action)
    {
    case GLFW_PRESS:
        event.Type = Event::KEY_PRESSED;
        break;
    case GLFW_RELEASE:
        event.Type = Event::KEY_RELEASED;
        break;
    case GLFW_REPEAT:
        event.Type = Event::KEY_REPEAT;
        break;
    default:
        break;
    }
    event.Key = static_cast<Input::Key>(p_Key);
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void cursorPositionCallback(GLFWwindow *p_Window, const double p_XPos, const double p_YPos)
{
    Event event;
    event.Type = Event::MOUSE_MOVED;
    event.Mouse.Position = {static_cast<f32>(p_XPos), static_cast<f32>(p_YPos)};
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void cursorEnterCallback(GLFWwindow *p_Window, const int p_Entered)
{
    Event event;
    event.Type = p_Entered ? Event::MOUSE_ENTERED : Event::MOUSE_LEFT;
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void mouseButtonCallback(GLFWwindow *p_Window, const int p_Button, const int p_Action, const int)
{
    Event event;
    event.Type = p_Action == GLFW_PRESS ? Event::MOUSE_PRESSED : Event::MOUSE_RELEASED;
    event.Mouse.Button = static_cast<Input::Mouse>(p_Button);
    windowFromGLFW(p_Window)->PushEvent(event);
}

static void scrollCallback(GLFWwindow *p_Window, double p_XOffset, double p_YOffset)
{
    Event event;
    event.Type = Event::SCROLLED;
    event.ScrollOffset = {static_cast<f32>(p_XOffset), static_cast<f32>(p_YOffset)};
    windowFromGLFW(p_Window)->PushEvent(event);
}

void Input::InstallCallbacks(Window &p_Window) noexcept
{
    glfwSetWindowSizeCallback(p_Window.GetWindowHandle(), windowResizeCallback);
    glfwSetWindowCloseCallback(p_Window.GetWindowHandle(), windowCloseCallback);
    glfwSetWindowFocusCallback(p_Window.GetWindowHandle(), windowFocusCallback);
    glfwSetKeyCallback(p_Window.GetWindowHandle(), keyCallback);
    glfwSetCursorPosCallback(p_Window.GetWindowHandle(), cursorPositionCallback);
    glfwSetCursorEnterCallback(p_Window.GetWindowHandle(), cursorEnterCallback);
    glfwSetMouseButtonCallback(p_Window.GetWindowHandle(), mouseButtonCallback);
    glfwSetScrollCallback(p_Window.GetWindowHandle(), scrollCallback);
}

} // namespace ONYX