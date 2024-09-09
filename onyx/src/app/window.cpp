#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/model/color.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
IWindow::IWindow() noexcept
{
    initialize();
}
IWindow::IWindow(const Specs &p_Specs) noexcept : m_Specs(p_Specs)
{
    initialize();
}

IWindow::~IWindow() noexcept
{
    m_Renderer.Release();
    vkDestroySurfaceKHR(m_Instance->VulkanInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void IWindow::initialize() noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
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

bool IWindow::Display() noexcept
{
    return Display([](const VkCommandBuffer) {});
}

void IWindow::MakeContextCurrent() const noexcept
{
    glfwMakeContextCurrent(m_Window);
}

bool IWindow::ShouldClose() const noexcept
{
    return glfwWindowShouldClose(m_Window);
}

const GLFWwindow *IWindow::GLFWWindow() const noexcept
{
    return m_Window;
}
GLFWwindow *IWindow::GLFWWindow() noexcept
{
    return m_Window;
}

const char *IWindow::Name() const noexcept
{
    return m_Specs.Name;
}

u32 IWindow::ScreenWidth() const noexcept
{
    return m_Specs.Width;
}
u32 IWindow::ScreenHeight() const noexcept
{
    return m_Specs.Height;
}

u32 IWindow::PixelWidth() const noexcept
{
    return m_Renderer->GetSwapChain().Width();
}
u32 IWindow::PixelHeight() const noexcept
{
    return m_Renderer->GetSwapChain().Height();
}

f32 IWindow::ScreenAspect() const noexcept
{
    return static_cast<f32>(m_Specs.Width) / static_cast<f32>(m_Specs.Height);
}

f32 IWindow::PixelAspect() const noexcept
{
    return m_Renderer->GetSwapChain().AspectRatio();
}

bool IWindow::WasResized() const noexcept
{
    return m_Resized;
}

void IWindow::FlagResize(const u32 p_Width, const u32 p_Height) noexcept
{
    m_Specs.Width = p_Width;
    m_Specs.Height = p_Height;
    m_Resized = true;
}
void IWindow::FlagResizeDone() noexcept
{
    m_Resized = false;
}

VkSurfaceKHR IWindow::Surface() const noexcept
{
    return m_Surface;
}

void IWindow::PushEvent(const Event &p_Event) noexcept
{
    m_Events.push_back(p_Event);
}

Event IWindow::PopEvent() noexcept
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

const Renderer &IWindow::GetRenderer() const noexcept
{
    return *m_Renderer;
}

ONYX_DIMENSION_TEMPLATE void Window<N>::runRenderSystems() noexcept
{
}

template class Window<2>;
template class Window<3>;
} // namespace ONYX