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

ONYX_DIMENSION_TEMPLATE static void window_resize_callback(GLFWwindow *p_Window, const int width, const int height)
{
    Event event;
    event.Type = Event::WINDOW_RESIZED;

    ;

    event.Window.OldWidth = win->ScreenWidth();
    event.Window.OldHeight = win->ScreenHeight();

    event.Window.NewWidth = (std::uint32_t)width;
    event.Window.NewHeight = (std::uint32_t)height;

    win->resize(event.Window.NewWidth, event.Window.NewHeight);
    win->push_event(event);
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
    event.Key = static_cast<Input::Key>(key);
    from_glfw<window<Dim>>(p_Window)->push_event(event);
}

ONYX_DIMENSION_TEMPLATE static void cursor_position_callback(GLFWwindow *p_Window, const double xpos, const double ypos)
{
    Event event;
    event.Type = Event::MOUSE_MOVED;
    event.Mouse.position = {(float)xpos, (float)ypos};
    from_glfw<window<Dim>>(p_Window)->push_event(event);
}

ONYX_DIMENSION_TEMPLATE static void mouse_button_callback(GLFWwindow *p_Window, const int button, const int action,
                                                          const int mods)
{
    Event event;
    event.Type = action == GLFW_PRESS ? Event::MOUSE_PRESSED : Event::MOUSE_RELEASED;
    event.Mouse.button = (typename input<Dim>::mouse)button;
    from_glfw<window<Dim>>(p_Window)->push_event(event);
}

ONYX_DIMENSION_TEMPLATE static void scroll_callback(GLFWwindow *p_Window, double xoffset, double yoffset)
{
    Event event;
    event.Type = Event::SCROLLED;
    event.ScrollOffset = {(float)xoffset, (float)yoffset};
    from_glfw<window<Dim>>(p_Window)->push_event(event);
}

void input<Dim>::install_callbacks(window_t *win)
{
    glfwSetWindowSizeCallback(win->glfw_window(), window_resize_callback<Dim>);
    glfwSetKeyCallback(win->glfw_window(), key_callback<Dim>);
    glfwSetCursorPosCallback(win->glfw_window(), cursor_position_callback<Dim>);
    glfwSetMouseButtonCallback(win->glfw_window(), mouse_button_callback<Dim>);
    glfwSetScrollCallback(win->glfw_window(), scroll_callback<Dim>);
}

template void Input::InstallCallbacks<2>(const Window<2> &p_Window) noexcept;
template void Input::InstallCallbacks<3>(const Window<3> &p_Window) noexcept;

} // namespace ONYX