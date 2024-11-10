#include "onyx/core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/color.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{

Window::Window() noexcept : Window(Specs{})
{
}

Window::Window(const Specs &p_Specs) noexcept : m_Name(p_Specs.Name), m_Width(p_Specs.Width), m_Height(p_Specs.Height)
{
    createWindow(p_Specs);

    m_RenderContext2D.Create(this, m_FrameScheduler->GetSwapChain().GetRenderPass());
    m_RenderContext3D.Create(this, m_FrameScheduler->GetSwapChain().GetRenderPass());
}

Window::~Window() noexcept
{
    m_FrameScheduler.Destroy();
    m_RenderContext2D.Destroy();
    m_RenderContext3D.Destroy();
    vkDestroySurfaceKHR(m_Instance->GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow(const Specs &p_Specs) noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Flags & Flags::RESIZABLE);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Flags & Flags::VISIBLE);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Flags & Flags::DECORATED);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Flags & Flags::FOCUSED);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Flags & Flags::FLOATING);

    m_Window = glfwCreateWindow(static_cast<i32>(p_Specs.Width), static_cast<i32>(p_Specs.Height), p_Specs.Name,
                                nullptr, nullptr);
    KIT_ASSERT(m_Window, "Failed to create a GLFW window");

    m_Instance = Core::GetInstance();
    KIT_ASSERT_RETURNS(glfwCreateWindowSurface(m_Instance->GetInstance(), m_Window, nullptr, &m_Surface), VK_SUCCESS,
                       "Failed to create a window surface");
    glfwSetWindowUserPointer(m_Window, this);

    m_Device = Core::tryCreateDevice(m_Surface);
    m_FrameScheduler.Create(*this);
    Input::InstallCallbacks(*this);
}

// I could define my own topology enum and use it here as the index... but i didnt

bool Window::Render() noexcept
{
    return Render([](const VkCommandBuffer) {}, [](const VkCommandBuffer) {});
}

bool Window::ShouldClose() const noexcept
{
    return glfwWindowShouldClose(m_Window);
}

const GLFWwindow *Window::GetWindowHandle() const noexcept
{
    return m_Window;
}
GLFWwindow *Window::GetWindowHandle() noexcept
{
    return m_Window;
}

const char *Window::GetName() const noexcept
{
    return m_Name;
}

u32 Window::GetScreenWidth() const noexcept
{
    return m_Width;
}
u32 Window::GetScreenHeight() const noexcept
{
    return m_Height;
}

u32 Window::GetPixelWidth() const noexcept
{
    return m_FrameScheduler->GetSwapChain().GetWidth();
}
u32 Window::GetPixelHeight() const noexcept
{
    return m_FrameScheduler->GetSwapChain().GetHeight();
}

f32 Window::GetScreenAspect() const noexcept
{
    return static_cast<f32>(m_Width) / static_cast<f32>(m_Height);
}

f32 Window::GetPixelAspect() const noexcept
{
    return m_FrameScheduler->GetSwapChain().GetAspectRatio();
}

std::pair<u32, u32> Window::GetPosition() const noexcept
{
    i32 x, y;
    glfwGetWindowPos(m_Window, &x, &y);
    return {static_cast<u32>(x), static_cast<u32>(y)};
}

bool Window::WasResized() const noexcept
{
    return m_Resized;
}

void Window::FlagResize(const u32 p_Width, const u32 p_Height) noexcept
{
    m_Width = p_Width;
    m_Height = p_Height;
    m_Resized = true;
}
void Window::FlagResizeDone() noexcept
{
    m_Resized = false;
}

void Window::FlagShouldClose() noexcept
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

VkSurfaceKHR Window::GetSurface() const noexcept
{
    return m_Surface;
}

void Window::PushEvent(const Event &p_Event) noexcept
{
    m_Events.push_back(p_Event);
}

const DynamicArray<Event> &Window::GetNewEvents() const noexcept
{
    return m_Events;
}
void Window::FlushEvents() noexcept
{
    m_Events.clear();
}

const FrameScheduler &Window::GetFrameScheduler() const noexcept
{
    return *m_FrameScheduler;
}
FrameScheduler &Window::GetFrameScheduler() noexcept
{
    return *m_FrameScheduler;
}

} // namespace ONYX