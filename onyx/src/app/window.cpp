#include "core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/draw/drawable.hpp"
#include "kit/core/logging.hpp"

namespace ONYX
{
struct GlobalUBO
{
    glm::mat4 Projection;
};

Window::Window() noexcept : Window(Specs{})
{
}

Window::Window(const Specs &p_Specs) noexcept : m_Name(p_Specs.Name), m_Width(p_Specs.Width), m_Height(p_Specs.Height)
{
    createWindow(p_Specs);
    createGlobalUniformHelper();
    addDefaultRenderSystems<2>();
    addDefaultRenderSystems<3>();
}

Window::~Window() noexcept
{
    m_Renderer.Release();
    vkDestroySurfaceKHR(m_Instance->GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow(const Specs &p_Specs) noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Resizable);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Visible);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Decorated);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Focused);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Floating);

    m_Window = glfwCreateWindow(static_cast<int>(p_Specs.Width), static_cast<int>(p_Specs.Height), p_Specs.Name,
                                nullptr, nullptr);
    KIT_ASSERT(m_Window, "Failed to create a GLFW window");

    m_Instance = Core::GetInstance();
    KIT_ASSERT_RETURNS(glfwCreateWindowSurface(m_Instance->GetInstance(), m_Window, nullptr, &m_Surface), VK_SUCCESS,
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

    const auto &props = m_Device->GetProperties();
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
        DescriptorWriter writer{&m_GlobalUniformHelper->Layout, &m_GlobalUniformHelper->Pool};
        writer.WriteBuffer(0, &info);
        m_GlobalDescriptorSets[i] = writer.Build();
    }
}

ONYX_DIMENSION_TEMPLATE void Window::addDefaultRenderSystems() noexcept
{
    RenderSystem::Specs<N> specs{};
    specs.RenderPass = m_Renderer->GetSwapChain().GetRenderPass();
    specs.DescriptorSetLayout = m_GlobalUniformHelper->Layout.GetLayout();

    // Triangle list
    specs.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_RenderSystems.emplace_back(specs);

    // Triangle strip
    specs.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    m_RenderSystems.emplace_back(specs);

    // Line list
    specs.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    m_RenderSystems.emplace_back(specs);

    // Line strip
    specs.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    m_RenderSystems.emplace_back(specs);

    // Point list
    specs.Topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    m_RenderSystems.emplace_back(specs);
}

void Window::drawRenderSystems(const VkCommandBuffer p_CommandBuffer) noexcept
{
    const u32 frameIndex = m_Renderer->GetFrameIndex();
    GlobalUBO ubo{};
    m_Camera->UpdateMatrices();
    m_Camera->KeepAspectRatio(GetScreenAspect());
    ubo.Projection = m_Camera->GetProjection();

    m_GlobalUniformHelper->UniformBuffer.WriteAt(frameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(frameIndex);

    RenderSystem::DrawInfo info{};
    info.CommandBuffer = p_CommandBuffer;
    info.DescriptorSet = m_GlobalDescriptorSets[frameIndex];
    for (RenderSystem &rs : m_RenderSystems)
        rs.Display(info);
}

void Window::Draw(Drawable &p_Drawable) noexcept
{
    p_Drawable.Draw(*this);
}

ONYX_DIMENSION_TEMPLATE RenderSystem &Window::AddRenderSystem(RenderSystem::Specs<N> p_Specs) noexcept
{
    if (!p_Specs.RenderPass)
        p_Specs.RenderPass = m_Renderer->GetSwapChain().GetRenderPass();
    return m_RenderSystems.emplace_back(p_Specs);
}

RenderSystem &Window::AddRenderSystem2D(const RenderSystem::Specs2D &p_Specs) noexcept
{
    return AddRenderSystem<2>(p_Specs);
}
RenderSystem &Window::AddRenderSystem3D(const RenderSystem::Specs3D &p_Specs) noexcept
{
    return AddRenderSystem<3>(p_Specs);
}

const RenderSystem &Window::GetRenderSystem(const usize p_Index) const noexcept
{
    return m_RenderSystems[p_Index];
}
RenderSystem &Window::GetRenderSystem(const usize p_Index) noexcept
{
    return m_RenderSystems[p_Index];
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

const GLFWwindow *Window::GetWindow() const noexcept
{
    return m_Window;
}
GLFWwindow *Window::GetWindow() noexcept
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
    return m_Renderer->GetSwapChain().GetWidth();
}
u32 Window::GetPixelHeight() const noexcept
{
    return m_Renderer->GetSwapChain().GetHeight();
}

f32 Window::GetScreenAspect() const noexcept
{
    return static_cast<f32>(m_Width) / static_cast<f32>(m_Height);
}

f32 Window::GetPixelAspect() const noexcept
{
    return m_Renderer->GetSwapChain().GetAspectRatio();
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

VkSurfaceKHR Window::GetSurface() const noexcept
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