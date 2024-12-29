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

fvec2 GetMousePosition(Window *p_Window) noexcept
{
    GLFWwindow *window = p_Window->GetWindowHandle();
    double xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);
    return {2.f * static_cast<f32>(xPos) / p_Window->GetScreenWidth() - 1.f,
            2.f * static_cast<f32>(yPos) / p_Window->GetScreenHeight() - 1.f};
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
    return glfwGetKeyName(static_cast<i32>(p_Key), 0);
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

    window->FlagResize(event.WindowResize.NewWidth, event.WindowResize.NewHeight);
    window->PushEvent(event);

    const f32 aspect = window->GetScreenAspect();
    window->GetRenderContext<D2>()->UpdateViewAspect(aspect);
    window->GetRenderContext<D3>()->UpdateViewAspect(aspect);
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