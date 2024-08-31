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

ONYX_DIMENSION_TEMPLATE static void windowResizeCallback(GLFWwindow *p_Window, const int p_Width, const int p_Height)
{
    const u32 width = static_cast<u32>(p_Width);
    const u32 height = static_cast<u32>(p_Height);
    Window<N> *window = windowFromGLFW<N>(p_Window);

    window->FlagResize(width, height);
}

ONYX_DIMENSION_TEMPLATE void Input::InstallCallbacks(const Window<N> &p_Window) noexcept
{
    glfwSetWindowSizeCallback(p_Window.GetGLFWWindow(), windowResizeCallback<N>);
}

void Input::PollEvents()
{
    glfwPollEvents();
}

template void Input::InstallCallbacks<2>(const Window<2> &p_Window) noexcept;
template void Input::InstallCallbacks<3>(const Window<3> &p_Window) noexcept;

} // namespace ONYX