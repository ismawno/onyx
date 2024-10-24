#include "core/pch.hpp"
#include "onyx/rendering/frame_scheduler.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/color.hpp"
#include "kit/multiprocessing/task_manager.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
FrameScheduler::FrameScheduler(Window &p_Window) noexcept
{
    m_Device = Core::GetDevice();
    createSwapChain(p_Window);
    createCommandPool(p_Window.GetSurface());
    createCommandBuffers();
}

FrameScheduler::~FrameScheduler() noexcept
{
    if (m_PresentTask)
        m_PresentTask->WaitUntilFinished();
    // Must wait for the device. Windows/RenderContexts may be destroyed at runtime, and all its command buffers must
    // have finished

    // Lock the queues to prevent any other command buffers from being submitted
    m_Device->WaitIdle();
    vkFreeCommandBuffers(m_Device->GetDevice(), m_CommandPool, SwapChain::MFIF, m_CommandBuffers.data());
    vkDestroyCommandPool(m_Device->GetDevice(), m_CommandPool, nullptr);
}

VkCommandBuffer FrameScheduler::BeginFrame(Window &p_Window) noexcept
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

void FrameScheduler::EndFrame(Window &) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end a frame when there is no frame in progress");
    KIT_ASSERT_RETURNS(vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS, "Failed to end command buffer");

    KIT::ITaskManager *taskManager = Core::GetTaskManager();

    // TODO: Profile this task usage
    if (!m_PresentTask)
        m_PresentTask = taskManager->CreateAndSubmit([this](usize) {
            KIT_ASSERT_RETURNS(m_SwapChain->SubmitCommandBuffer(m_CommandBuffers[m_FrameIndex], m_ImageIndex),
                               VK_SUCCESS, "Failed to submit command buffers");
            m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
            return m_SwapChain->Present(&m_ImageIndex);
        });
    else
    {
        m_PresentTask->Reset();
        taskManager->SubmitTask(m_PresentTask);
    }
    m_FrameStarted = false;
}

void FrameScheduler::BeginRenderPass(const Color &p_ClearColor) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot begin render pass if a frame is not in progress");
    const VkExtent2D extent = m_SwapChain->GetExtent();

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_SwapChain->GetRenderPass();
    passInfo.framebuffer = m_SwapChain->GetFrameBuffer(m_ImageIndex);
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
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {extent.width, extent.height};

    vkCmdSetViewport(m_CommandBuffers[m_FrameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_CommandBuffers[m_FrameIndex], 0, 1, &scissor);
}

void FrameScheduler::EndRenderPass() noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end render pass if a frame is not in progress");
    vkCmdEndRenderPass(m_CommandBuffers[m_FrameIndex]);
}

u32 FrameScheduler::GetFrameIndex() const noexcept
{
    return m_FrameIndex;
}

VkCommandBuffer FrameScheduler::GetCurrentCommandBuffer() const noexcept
{
    return m_CommandBuffers[m_FrameIndex];
}

const SwapChain &FrameScheduler::GetSwapChain() const noexcept
{
    return *m_SwapChain;
}

void FrameScheduler::createSwapChain(Window &p_Window) noexcept
{
    VkExtent2D windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
        glfwWaitEvents();
    }
    m_Device->WaitIdle();
    m_SwapChain = KIT::Scope<SwapChain>::Create(windowExtent, p_Window.GetSurface(), m_SwapChain.Get());
}

void FrameScheduler::createCommandPool(const VkSurfaceKHR p_Surface) noexcept
{
    const Device::QueueFamilyIndices indices = m_Device->FindQueueFamilies(p_Surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.GraphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    KIT_ASSERT_RETURNS(vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_CommandPool), VK_SUCCESS,
                       "Failed to create command pool");
}

void FrameScheduler::createCommandBuffers() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = SwapChain::MFIF;

    KIT_ASSERT_RETURNS(vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, m_CommandBuffers.data()), VK_SUCCESS,
                       "Failed to create command buffers");
}

} // namespace ONYX