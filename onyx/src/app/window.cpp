#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Window<N>::Window() noexcept
{
    initializeWindow();
}
ONYX_DIMENSION_TEMPLATE Window<N>::Window(const Specs &p_Specs) noexcept : m_Specs(p_Specs)
{
    initializeWindow();
}

ONYX_DIMENSION_TEMPLATE Window<N>::~Window() noexcept
{
    vkDestroySurfaceKHR(m_Instance->VulkanInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

ONYX_DIMENSION_TEMPLATE void Window<N>::initializeWindow() noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(static_cast<int>(m_Specs.width), static_cast<int>(m_Specs.height), m_Specs.name,
                                nullptr, nullptr);
    KIT_ASSERT(m_Window, "Failed to create a GLFW window");

    m_Instance = Core::GetInstance();
    KIT_ASSERT_RETURNS(glfwCreateWindowSurface(m_Instance->VulkanInstance(), m_Window, nullptr, &m_Surface), VK_SUCCESS,
                       "Failed to create a window surface");

    m_Device = Core::getOrCreateDevice(m_Surface);
}

ONYX_DIMENSION_TEMPLATE bool Window<N>::ShouldClose() const noexcept
{
    return glfwWindowShouldClose(m_Window);
}

ONYX_DIMENSION_TEMPLATE GLFWwindow *Window<N>::GetGLFWWindow() const noexcept
{
    return m_Window;
}

ONYX_DIMENSION_TEMPLATE const char *Window<N>::Name() const noexcept
{
    return m_Specs.name;
}
ONYX_DIMENSION_TEMPLATE u32 Window<N>::Width() const noexcept
{
    return m_Specs.width;
}
ONYX_DIMENSION_TEMPLATE u32 Window<N>::Height() const noexcept
{
    return m_Specs.height;
}

ONYX_DIMENSION_TEMPLATE VkSurfaceKHR Window<N>::Surface() const noexcept
{
    return m_Surface;
}

template class Window<2>;
template class Window<3>;
} // namespace ONYX