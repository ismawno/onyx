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
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Flags & Flags::RESIZABLE);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Flags & Flags::VISIBLE);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Flags & Flags::DECORATED);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Flags & Flags::FOCUSED);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Flags & Flags::FLOATING);

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
    bufferSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
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
    m_Camera->SetAspectRatio(GetScreenAspect());
    ubo.Projection = m_Camera->GetProjection();

    m_GlobalUniformHelper->UniformBuffer.WriteAt(frameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(frameIndex);

    RenderSystem::DrawInfo info{};
    info.CommandBuffer = p_CommandBuffer;
    info.DescriptorSet = m_GlobalDescriptorSets[frameIndex];
    for (RenderSystem &rs : m_RenderSystems)
    {
        rs.Display(info);
        rs.ClearRenderData();
    }
}

void Window::Draw(Drawable &p_Drawable) noexcept
{
    p_Drawable.Draw(*this);
}

void Window::Draw(Window &p_Window) noexcept
{
    KIT_ASSERT(this != &p_Window, "Cannot draw a window to itself");
    KIT_ASSERT(p_Window.m_RenderSystems.size() >= m_RenderSystems.size(),
               "The window to draw must have at least the same amount of render systems as the current window");

    // A render system cannot be deleted, so we can safely assume that the render systems are in the same order
    for (usize i = 0; i < m_RenderSystems.size(); ++i)
        m_RenderSystems[i].SubmitRenderData(p_Window.m_RenderSystems[i]);
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

ONYX_DIMENSION_TEMPLATE const RenderSystem *Window::GetRenderSystem(VkPrimitiveTopology p_Topology) const noexcept
{
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        return &m_RenderSystems[0 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        return &m_RenderSystems[1 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        return &m_RenderSystems[2 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
        return &m_RenderSystems[3 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
        return &m_RenderSystems[4 + (N - 2) * 5];
    return nullptr;
}

ONYX_DIMENSION_TEMPLATE RenderSystem *Window::GetRenderSystem(VkPrimitiveTopology p_Topology) noexcept
{
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        return &m_RenderSystems[0 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        return &m_RenderSystems[1 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        return &m_RenderSystems[2 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
        return &m_RenderSystems[3 + (N - 2) * 5];
    if (p_Topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
        return &m_RenderSystems[4 + (N - 2) * 5];
    return nullptr;
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

const Renderer &Window::GetRenderer() const noexcept
{
    return *m_Renderer;
}

template const RenderSystem *Window::GetRenderSystem<2>(VkPrimitiveTopology) const noexcept;
template const RenderSystem *Window::GetRenderSystem<3>(VkPrimitiveTopology) const noexcept;

template RenderSystem *Window::GetRenderSystem<2>(VkPrimitiveTopology) noexcept;
template RenderSystem *Window::GetRenderSystem<3>(VkPrimitiveTopology) noexcept;

} // namespace ONYX