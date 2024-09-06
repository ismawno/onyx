#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/model/color.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Window<N>::Window() noexcept
{
    initialize();
}
ONYX_DIMENSION_TEMPLATE Window<N>::Window(const Specs &p_Specs) noexcept : m_Specs(p_Specs)
{
    initialize();
}

ONYX_DIMENSION_TEMPLATE Window<N>::~Window() noexcept
{
    m_Renderer.Release();
    vkDestroySurfaceKHR(m_Instance->VulkanInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

ONYX_DIMENSION_TEMPLATE void Window<N>::initialize() noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = glfwCreateWindow(static_cast<int>(m_Specs.Width), static_cast<int>(m_Specs.Height), m_Specs.Name,
                                nullptr, nullptr);
    KIT_ASSERT(m_Window, "Failed to create a GLFW window");

    m_Instance = Core::Instance();
    KIT_ASSERT_RETURNS(glfwCreateWindowSurface(m_Instance->VulkanInstance(), m_Window, nullptr, &m_Surface), VK_SUCCESS,
                       "Failed to create a window surface");
    glfwSetWindowUserPointer(m_Window, this);

    m_Device = Core::tryCreateDevice(m_Surface);
    m_Renderer = KIT::Scope<Renderer>::Create(*this);
    Input::InstallCallbacks(*this);
}

ONYX_DIMENSION_TEMPLATE void Window<N>::Display() noexcept
{
    if (m_Renderer->BeginFrame(*this))
    {
        m_Renderer->BeginRenderPass(BackgroundColor);
        m_Renderer->EndRenderPass();
        m_Renderer->EndFrame(*this);
    }
}

ONYX_DIMENSION_TEMPLATE void Window<N>::MakeContextCurrent() const noexcept
{
    glfwMakeContextCurrent(m_Window);
}

ONYX_DIMENSION_TEMPLATE bool Window<N>::ShouldClose() const noexcept
{
    return glfwWindowShouldClose(m_Window);
}

ONYX_DIMENSION_TEMPLATE const GLFWwindow *Window<N>::GetGLFWWindow() const noexcept
{
    return m_Window;
}
ONYX_DIMENSION_TEMPLATE GLFWwindow *Window<N>::GetGLFWWindow() noexcept
{
    return m_Window;
}

ONYX_DIMENSION_TEMPLATE const char *Window<N>::Name() const noexcept
{
    return m_Specs.Name;
}

ONYX_DIMENSION_TEMPLATE u32 Window<N>::ScreenWidth() const noexcept
{
    return m_Specs.Width;
}
ONYX_DIMENSION_TEMPLATE u32 Window<N>::ScreenHeight() const noexcept
{
    return m_Specs.Height;
}

ONYX_DIMENSION_TEMPLATE u32 Window<N>::PixelWidth() const noexcept
{
    return m_Renderer->GetSwapChain().Width();
}
ONYX_DIMENSION_TEMPLATE u32 Window<N>::PixelHeight() const noexcept
{
    return m_Renderer->GetSwapChain().Height();
}

ONYX_DIMENSION_TEMPLATE f32 Window<N>::ScreenAspect() const noexcept
{
    return static_cast<f32>(m_Specs.Width) / static_cast<f32>(m_Specs.Height);
}

ONYX_DIMENSION_TEMPLATE f32 Window<N>::PixelAspect() const noexcept
{
    return m_Renderer->GetSwapChain().AspectRatio();
}

ONYX_DIMENSION_TEMPLATE bool Window<N>::WasResized() const noexcept
{
    return m_Resized;
}

ONYX_DIMENSION_TEMPLATE void Window<N>::FlagResize(const u32 p_Width, const u32 p_Height) noexcept
{
    m_Specs.Width = p_Width;
    m_Specs.Height = p_Height;
    m_Resized = true;
}
ONYX_DIMENSION_TEMPLATE void Window<N>::FlagResizeDone() noexcept
{
    m_Resized = false;
}

ONYX_DIMENSION_TEMPLATE VkSurfaceKHR Window<N>::Surface() const noexcept
{
    return m_Surface;
}

ONYX_DIMENSION_TEMPLATE void Window<N>::PushEvent(const Event &p_Event) noexcept
{
    m_Events.push_back(p_Event);
}

ONYX_DIMENSION_TEMPLATE Event Window<N>::PopEvent() noexcept
{
    Event event;
    if (m_Events.empty())
    {
        event.Empty = true;
        return event;
    }
    event = m_Events.front();
    m_Events.pop_front();
    return event;
}

template class Window<2>;
template class Window<3>;
} // namespace ONYX