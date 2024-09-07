#include "core/pch.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE static Window<N> *windowFromGLFW(GLFWwindow *p_Window)
{
    return static_cast<Window<N> *>(glfwGetWindowUserPointer(p_Window));
}

void Input::PollEvents()
{
    glfwPollEvents();
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

const char *Input::KeyName(const Key p_Key) noexcept
{
    return glfwGetKeyName(static_cast<int>(p_Key), 0);
}

ONYX_DIMENSION_TEMPLATE static void windowResizeCallback(GLFWwindow *p_Window, const int p_Width, const int p_Height)
{
    Event event;
    event.Type = Event::WINDOW_RESIZED;

    Window<N> *window = windowFromGLFW<N>(p_Window);

    event.Window.OldWidth = window->ScreenWidth();
    event.Window.OldHeight = window->ScreenHeight();

    event.Window.NewWidth = static_cast<u32>(p_Width);
    event.Window.NewHeight = static_cast<u32>(p_Height);

    window->FlagResize(event.Window.NewWidth, event.Window.NewHeight);
    window->PushEvent(event);
}

ONYX_DIMENSION_TEMPLATE static void keyCallback(GLFWwindow *p_Window, const int p_Key, const int, const int p_Action,
                                                const int)
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
    windowFromGLFW<N>(p_Window)->PushEvent(event);
}

ONYX_DIMENSION_TEMPLATE static void cursorPositionCallback(GLFWwindow *p_Window, const double p_XPos,
                                                           const double p_YPos)
{
    Event event;
    event.Type = Event::MOUSE_MOVED;
    event.Mouse.Position = {static_cast<f32>(p_XPos), static_cast<f32>(p_YPos)};
    windowFromGLFW<N>(p_Window)->PushEvent(event);
}

ONYX_DIMENSION_TEMPLATE static void mouseButtonCallback(GLFWwindow *p_Window, const int p_Button, const int p_Action,
                                                        const int)
{
    Event event;
    event.Type = p_Action == GLFW_PRESS ? Event::MOUSE_PRESSED : Event::MOUSE_RELEASED;
    event.Mouse.Button = static_cast<Input::Mouse>(p_Button);
    windowFromGLFW<N>(p_Window)->PushEvent(event);
}

ONYX_DIMENSION_TEMPLATE static void scrollCallback(GLFWwindow *p_Window, double p_XOffset, double p_YOffset)
{
    Event event;
    event.Type = Event::SCROLLED;
    event.ScrollOffset = {static_cast<f32>(p_XOffset), static_cast<f32>(p_YOffset)};
    windowFromGLFW<N>(p_Window)->PushEvent(event);
}

ONYX_DIMENSION_TEMPLATE void Input::InstallCallbacks(Window<N> &p_Window) noexcept
{
    glfwSetWindowSizeCallback(p_Window.GLFWWindow(), windowResizeCallback<N>);
    glfwSetKeyCallback(p_Window.GLFWWindow(), keyCallback<N>);
    glfwSetCursorPosCallback(p_Window.GLFWWindow(), cursorPositionCallback<N>);
    glfwSetMouseButtonCallback(p_Window.GLFWWindow(), mouseButtonCallback<N>);
    glfwSetScrollCallback(p_Window.GLFWWindow(), scrollCallback<N>);
}

template void Input::InstallCallbacks<2>(Window<2> &p_Window) noexcept;
template void Input::InstallCallbacks<3>(Window<3> &p_Window) noexcept;

} // namespace ONYX