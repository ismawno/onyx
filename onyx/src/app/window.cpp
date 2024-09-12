#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/model/color.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
Window::Window() noexcept
{
    initialize();
}
Window::Window(const Specs &p_Specs) noexcept : m_Specs(p_Specs)
{
    initialize();
}

Window::~Window() noexcept
{
    m_Renderer.Release();
    vkDestroySurfaceKHR(m_Instance->VulkanInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::initialize() noexcept
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

void Window::drawRenderSystems() noexcept
{
}

bool Window::Display() noexcept
{
    return Display([](const VkCommandBuffer) {});
}

void Window::MakeContextCurrent() const noexcept
{
    glfwMakeContextCurrent(m_Window);
}

bool Window::ShouldClose() const noexcept
{
    return glfwWindowShouldClose(m_Window);
}

const GLFWwindow *Window::GLFWWindow() const noexcept
{
    return m_Window;
}
GLFWwindow *Window::GLFWWindow() noexcept
{
    return m_Window;
}

const char *Window::Name() const noexcept
{
    return m_Specs.Name;
}

u32 Window::ScreenWidth() const noexcept
{
    return m_Specs.Width;
}
u32 Window::ScreenHeight() const noexcept
{
    return m_Specs.Height;
}

u32 Window::PixelWidth() const noexcept
{
    return m_Renderer->GetSwapChain().Width();
}
u32 Window::PixelHeight() const noexcept
{
    return m_Renderer->GetSwapChain().Height();
}

f32 Window::ScreenAspect() const noexcept
{
    return static_cast<f32>(m_Specs.Width) / static_cast<f32>(m_Specs.Height);
}

f32 Window::PixelAspect() const noexcept
{
    return m_Renderer->GetSwapChain().AspectRatio();
}

bool Window::WasResized() const noexcept
{
    return m_Resized;
}

void Window::FlagResize(const u32 p_Width, const u32 p_Height) noexcept
{
    m_Specs.Width = p_Width;
    m_Specs.Height = p_Height;
    m_Resized = true;
}
void Window::FlagResizeDone() noexcept
{
    m_Resized = false;
}

VkSurfaceKHR Window::Surface() const noexcept
{
    return m_Surface;
}

void Window::PushEvent(const Event &p_Event) noexcept
{
    m_Events.push_back(p_Event);
}

Event Window::PopEvent() noexcept
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

const Renderer &Window::GetRenderer() const noexcept
{
    return *m_Renderer;
}

} // namespace ONYX