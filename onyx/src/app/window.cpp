#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "kit/core/logging.hpp"

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE Window<N>::Window() KIT_NOEXCEPT
{
    InitializeWindow();
}
ONYX_DIMENSION_TEMPLATE Window<N>::Window(const Specs &specs) KIT_NOEXCEPT : m_Specs(specs)
{
    InitializeWindow();
}

ONYX_DIMENSION_TEMPLATE Window<N>::~Window() KIT_NOEXCEPT
{
    glfwDestroyWindow(m_Window);
}

ONYX_DIMENSION_TEMPLATE void Window<N>::InitializeWindow() KIT_NOEXCEPT
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(static_cast<int>(m_Specs.width), static_cast<int>(m_Specs.height), m_Specs.name,
                                nullptr, nullptr);
    KIT_ASSERT(m_Window, "Failed to create a GLFW window");
}

ONYX_DIMENSION_TEMPLATE bool Window<N>::ShouldClose() const KIT_NOEXCEPT
{
    return glfwWindowShouldClose(m_Window);
}

template class Window<2>;
template class Window<3>;

ONYX_NAMESPACE_END