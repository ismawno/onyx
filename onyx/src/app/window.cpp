#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/model/color.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
struct GlobalUBO
{
    glm::mat4 Projection;
};

Window::Window() noexcept
{
    createWindow();
    createGlobalUniformHelper();
}
Window::Window(const Specs &p_Specs) noexcept : m_Specs(p_Specs)
{
    createWindow();
    createGlobalUniformHelper();
}

Window::~Window() noexcept
{
    m_Renderer.Release();
    vkDestroySurfaceKHR(m_Instance->VulkanInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow() noexcept
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

void Window::createGlobalUniformHelper() noexcept
{
    DescriptorPool::Specs poolSpecs{};
    poolSpecs.MaxSets = SwapChain::MAX_FRAMES_IN_FLIGHT;
    poolSpecs.PoolSizes.push_back(
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT});

    static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
        {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}}};

    const auto &props = m_Device->Properties();
    Buffer::Specs bufferSpecs{};
    bufferSpecs.InstanceCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
    bufferSpecs.InstanceSize = sizeof(GlobalUBO);
    bufferSpecs.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferSpecs.Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    bufferSpecs.MinimumAlignment =
        glm::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);

    m_GlobalUniformHelper = KIT::Scope<GlobalUniformHelper>::Create(poolSpecs, bindings, bufferSpecs);
    m_GlobalUniformHelper->UniformBuffer.Map();

    for (usize i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const auto info = m_GlobalUniformHelper->UniformBuffer.DescriptorInfoAt(i);
        DescriptorWriter writer{&m_GlobalUniformHelper->Layouts, &m_GlobalUniformHelper->Pool};
        writer.WriteBuffer(0, &info);
        m_GlobalDescriptorSets[i] = writer.Build();
    }
}

void Window::drawRenderSystems() noexcept
{
    const u32 frameIndex = m_Renderer->FrameIndex();
    GlobalUBO ubo{};

    m_GlobalUniformHelper->UniformBuffer.WriteAt(frameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(frameIndex);
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