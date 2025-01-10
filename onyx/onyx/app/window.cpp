#include "onyx/core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/color.hpp"
#include "tkit/core/logging.hpp"

namespace Onyx
{

Window::Window() noexcept : Window(Specs{})
{
}

Window::Window(const Specs &p_Specs) noexcept : m_Name(p_Specs.Name), m_Width(p_Specs.Width), m_Height(p_Specs.Height)
{
    createWindow(p_Specs);

    m_RenderContext2D.Construct(this, m_FrameScheduler->GetRenderPass());
    m_RenderContext3D.Construct(this, m_FrameScheduler->GetRenderPass());
}

Window::~Window() noexcept
{
    m_FrameScheduler.Destruct();
    m_RenderContext2D.Destruct();
    m_RenderContext3D.Destruct();
    vkDestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow(const Specs &p_Specs) noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Flags & Flag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Flags & Flag_Visible);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Flags & Flag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Flags & Flag_Focused);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Flags & Flag_Floating);

    m_Window = glfwCreateWindow(static_cast<i32>(p_Specs.Width), static_cast<i32>(p_Specs.Height), p_Specs.Name,
                                nullptr, nullptr);
    TKIT_ASSERT(m_Window, "[ONYX] Failed to create a GLFW window");

    TKIT_ASSERT_RETURNS(glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface), VK_SUCCESS,
                        "[ONYX] Failed to create a window surface");
    glfwSetWindowUserPointer(m_Window, this);

    if (!Core::IsDeviceCreated())
        Core::CreateDevice(m_Surface);
    m_FrameScheduler.Construct(*this);
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
    return m_FrameScheduler->GetSwapChain().GetInfo().Extent.width;
}
u32 Window::GetPixelHeight() const noexcept
{
    return m_FrameScheduler->GetSwapChain().GetInfo().Extent.height;
}

f32 Window::GetScreenAspect() const noexcept
{
    return static_cast<f32>(m_Width) / static_cast<f32>(m_Height);
}

f32 Window::GetPixelAspect() const noexcept
{
    return static_cast<f32>(GetPixelWidth()) / static_cast<f32>(GetPixelHeight());
}

std::pair<u32, u32> Window::GetPosition() const noexcept
{
    i32 x, y;
    glfwGetWindowPos(m_Window, &x, &y);
    return {static_cast<u32>(x), static_cast<u32>(y)};
}

PostProcessing *Window::SetupPostProcessing(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
                                            const VKit::Shader *p_VertexShader,
                                            const VkSamplerCreateInfo *p_Info) noexcept
{
    return m_FrameScheduler->SetupPostProcessing(p_Layout, p_FragmentShader, p_VertexShader, p_Info);
}

PostProcessing *Window::GetPostProcessing() noexcept
{
    return m_FrameScheduler->GetPostProcessing();
}

void Window::RemovePostProcessing() noexcept
{
    m_FrameScheduler->RemovePostProcessing();
}

bool Window::wasResized() const noexcept
{
    return m_Resized;
}

void Window::flagResize(const u32 p_Width, const u32 p_Height) noexcept
{
    m_Width = p_Width;
    m_Height = p_Height;
    m_Resized = true;
}
void Window::flagResizeDone() noexcept
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
    if (!m_Events.full())
        m_Events.push_back(p_Event);
}

const TKit::StaticArray128<Event> &Window::GetNewEvents() const noexcept
{
    return m_Events;
}
void Window::FlushEvents() noexcept
{
    m_Events.clear();
}

const VKit::RenderPass &Window::GetRenderPass() const noexcept
{
    return m_FrameScheduler->GetRenderPass();
}

u32 Window::GetFrameIndex() const noexcept
{
    return m_FrameScheduler->GetFrameIndex();
}

VkPresentModeKHR Window::GetPresentMode() const noexcept
{
    return m_FrameScheduler->GetPresentMode();
}
const TKit::StaticArray8<VkPresentModeKHR> &Window::GetAvailablePresentModes() const noexcept
{
    return m_FrameScheduler->GetSwapChain().GetInfo().SupportDetails.PresentModes;
}

void Window::SetPresentMode(VkPresentModeKHR p_PresentMode) noexcept
{
    m_FrameScheduler->SetPresentMode(p_PresentMode);
}

} // namespace Onyx