#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/core.hpp"
#include "onyx/model/color.hpp"
#include "kit/multiprocessing/task_manager.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
Renderer::Renderer(IWindow &p_Window) noexcept
{
    m_Device = Core::Device();
    createSwapChain(p_Window);
    createCommandPool(p_Window.Surface());
    createCommandBuffers();
}

Renderer::~Renderer() noexcept
{
    m_PresentTask->WaitUntilFinished();
    // Must wait for the device. Windows/Renderers may be destroyed at runtime, and all its command buffers must have
    // finished

    // Lock the queues to prevent any other command buffers from being submitted
    m_Device->WaitIdle();
    vkFreeCommandBuffers(m_Device->VulkanDevice(), m_CommandPool, SwapChain::MAX_FRAMES_IN_FLIGHT,
                         m_CommandBuffers.data());
    vkDestroyCommandPool(m_Device->VulkanDevice(), m_CommandPool, nullptr);
}

VkCommandBuffer Renderer::BeginFrame(IWindow &p_Window) noexcept
{
    KIT_ASSERT(!m_FrameStarted, "Cannot begin a new frame when there is already one in progress");

    if (m_PresentTask)
    {
        const VkResult result = m_PresentTask->WaitForResult();
        const bool resizeFixes =
            result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || p_Window.WasResized();
        if (resizeFixes)
        {
            createSwapChain(p_Window);
            p_Window.FlagResizeDone();
        }
        KIT_ASSERT(resizeFixes || result == VK_SUCCESS, "Failed to submit command buffers");
    }

    const VkResult result = m_SwapChain->AcquireNextImage(&m_ImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        createSwapChain(p_Window);
        return VK_NULL_HANDLE;
    }

    KIT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image");
    m_FrameStarted = true;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    KIT_ASSERT_RETURNS(vkResetCommandBuffer(m_CommandBuffers[m_FrameIndex], 0), VK_SUCCESS,
                       "Failed to reset command buffer");
    KIT_ASSERT_RETURNS(vkBeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &beginInfo), VK_SUCCESS,
                       "Failed to begin command buffer");

    return m_CommandBuffers[m_FrameIndex];
}

void Renderer::EndFrame(IWindow &) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end a frame when there is no frame in progress");
    KIT_ASSERT_RETURNS(vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS, "Failed to end command buffer");

    KIT::TaskManager *taskManager = Core::TaskManager();

    // TODO: Profile this task usage
    if (!m_PresentTask)
        m_PresentTask = taskManager->CreateAndSubmit([this](usize) {
            KIT_ASSERT_RETURNS(m_SwapChain->SubmitCommandBuffer(m_CommandBuffers[m_FrameIndex], m_ImageIndex),
                               VK_SUCCESS, "Failed to submit command buffers");
            m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
            return m_SwapChain->Present(&m_ImageIndex);
        });
    else
    {
        m_PresentTask->Reset();
        taskManager->SubmitTask(m_PresentTask);
    }
    m_FrameStarted = false;
}

void Renderer::BeginRenderPass(const Color &p_ClearColor) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot begin render pass if a frame is not in progress");
    const VkExtent2D extent = m_SwapChain->Extent();

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_SwapChain->RenderPass();
    passInfo.framebuffer = m_SwapChain->FrameBuffer(m_ImageIndex);
    passInfo.renderArea.offset = {0, 0};
    passInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {{p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};
    clear_values[1].depthStencil = {1, 0};

    passInfo.clearValueCount = 2;
    passInfo.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(m_CommandBuffers[m_FrameIndex], &passInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {extent.width, extent.height};

    vkCmdSetViewport(m_CommandBuffers[m_FrameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_CommandBuffers[m_FrameIndex], 0, 1, &scissor);
}

void Renderer::EndRenderPass() noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end render pass if a frame is not in progress");
    vkCmdEndRenderPass(m_CommandBuffers[m_FrameIndex]);
}

VkCommandBuffer Renderer::CurrentCommandBuffer() const noexcept
{
    return m_CommandBuffers[m_FrameIndex];
}

const SwapChain &Renderer::GetSwapChain() const noexcept
{
    return *m_SwapChain;
}

void Renderer::createSwapChain(IWindow &p_Window) noexcept
{
    VkExtent2D windowExtent = {p_Window.ScreenWidth(), p_Window.ScreenHeight()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {p_Window.ScreenWidth(), p_Window.ScreenHeight()};
        glfwWaitEvents();
    }
    m_Device->WaitIdle();
    m_SwapChain = KIT::Scope<SwapChain>::Create(windowExtent, p_Window.Surface(), m_SwapChain.Get());
}

void Renderer::createCommandPool(const VkSurfaceKHR p_Surface) noexcept
{
    const Device::QueueFamilyIndices indices = m_Device->FindQueueFamilies(p_Surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.GraphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    KIT_ASSERT_RETURNS(vkCreateCommandPool(m_Device->VulkanDevice(), &poolInfo, nullptr, &m_CommandPool), VK_SUCCESS,
                       "Failed to create command pool");
}

void Renderer::createCommandBuffers() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = SwapChain::MAX_FRAMES_IN_FLIGHT;

    KIT_ASSERT_RETURNS(vkAllocateCommandBuffers(m_Device->VulkanDevice(), &allocInfo, m_CommandBuffers.data()),
                       VK_SUCCESS, "Failed to create command buffers");
}

} // namespace ONYX