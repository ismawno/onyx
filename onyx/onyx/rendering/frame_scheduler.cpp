#include "onyx/core/pch.hpp"
#include "onyx/rendering/frame_scheduler.hpp"
#include "onyx/app/window.hpp"
#include "onyx/draw/color.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

#include "onyx/core/glfw.hpp"

namespace Onyx
{
FrameScheduler::FrameScheduler(Window &p_Window) noexcept
{
    createSwapChain(p_Window);
    m_InFlightImages.resize(m_SwapChain.GetInfo().ImageData.size(), VK_NULL_HANDLE);
    createRenderPass();
    createCommandPool();
    createCommandBuffers();
}

FrameScheduler::~FrameScheduler() noexcept
{
    if (m_PresentTask)
        m_PresentTask->WaitUntilFinished();
    // Must wait for the device. Windows/RenderContexts may be destroyed at runtime, and all its command buffers must
    // have finished

    // Lock the queues to prevent any other command buffers from being submitted
    {
        std::scoped_lock lock{Core::GetGraphicsMutex()};
        vkQueueWaitIdle(Core::GetGraphicsQueue());
    }
    vkFreeCommandBuffers(Core::GetDevice(), m_CommandPool, VKIT_MAX_FRAMES_IN_FLIGHT, m_CommandBuffers.data());
    vkDestroyCommandPool(Core::GetDevice(), m_CommandPool, nullptr);
    vkDestroyRenderPass(Core::GetDevice(), m_RenderPass, nullptr);
    m_SwapChain.Destroy();
}

VkCommandBuffer FrameScheduler::BeginFrame(Window &p_Window) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::BeginFrame");
    TKIT_ASSERT(!m_FrameStarted, "Cannot begin a new frame when there is already one in progress");

    if (m_PresentTask) [[likely]]
    {
        const VkResult result = m_PresentTask->WaitForResult();
        const bool resizeFixes = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                                 p_Window.WasResized() || m_PresentModeChanged;
        if (resizeFixes)
        {
            createSwapChain(p_Window);
            p_Window.FlagResizeDone();
            m_PresentModeChanged = false;
        }
        TKIT_ASSERT(resizeFixes || result == VK_SUCCESS, "Failed to submit command buffers");
    }
    else
    {
        TKit::ITaskManager *taskManager = Core::GetTaskManager();
        m_PresentTask = taskManager->CreateTask([this](usize) {
            TKIT_ASSERT_RETURNS(SubmitCurrentCommandBuffer(), VK_SUCCESS, "Failed to submit command buffers");
            return Present();
        });
    }

    const VkResult result = AcquireNextImage();
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        createSwapChain(p_Window);
        return VK_NULL_HANDLE;
    }

    TKIT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image");
    m_FrameStarted = true;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    TKIT_ASSERT_RETURNS(vkResetCommandBuffer(m_CommandBuffers[m_FrameIndex], 0), VK_SUCCESS,
                        "Failed to reset command buffer");
    TKIT_ASSERT_RETURNS(vkBeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &beginInfo), VK_SUCCESS,
                        "Failed to begin command buffer");

    return m_CommandBuffers[m_FrameIndex];
}

void FrameScheduler::EndFrame(Window &) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::EndFrame");
    TKIT_ASSERT(m_FrameStarted, "Cannot end a frame when there is no frame in progress");
    TKIT_ASSERT_RETURNS(vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS, "Failed to end command buffer");

    TKit::ITaskManager *taskManager = Core::GetTaskManager();

    m_PresentTask->Reset();
    taskManager->SubmitTask(m_PresentTask);
    m_FrameStarted = false;
}

void FrameScheduler::BeginRenderPass(const Color &p_ClearColor) noexcept
{
    TKIT_ASSERT(m_FrameStarted, "Cannot begin render pass if a frame is not in progress");

    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const VkExtent2D extent = info.Extent;

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_RenderPass;
    passInfo.framebuffer = info.ImageData[m_ImageIndex].FrameBuffer;
    passInfo.renderArea.offset = {0, 0};
    passInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {{p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};
    clear_values[1].depthStencil.depth = 1.f;
    clear_values[1].depthStencil.stencil = 0;

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
    TKIT_ASSERT(m_FrameStarted, "Cannot end render pass if a frame is not in progress");
    vkCmdEndRenderPass(m_CommandBuffers[m_FrameIndex]);
}

VkResult FrameScheduler::AcquireNextImage() noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    vkWaitForFences(Core::GetDevice(), 1, &info.SyncData[m_FrameIndex].InFlightFence, VK_TRUE, UINT64_MAX);
    return vkAcquireNextImageKHR(Core::GetDevice(), m_SwapChain, UINT64_MAX,
                                 info.SyncData[m_FrameIndex].ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);
}
VkResult FrameScheduler::SubmitCurrentCommandBuffer() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::SubmitCurrentCommandBuffer");
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const VkCommandBuffer cmd = m_CommandBuffers[m_FrameIndex];

    if (m_InFlightImages[m_ImageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(Core::GetDevice(), 1, &m_InFlightImages[m_ImageIndex], VK_TRUE, UINT64_MAX);

    m_InFlightImages[m_ImageIndex] = info.SyncData[m_FrameIndex].InFlightFence;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &info.SyncData[m_FrameIndex].ImageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &info.SyncData[m_FrameIndex].RenderFinishedSemaphore;

    vkResetFences(Core::GetDevice(), 1, &info.SyncData[m_FrameIndex].InFlightFence);

    // A nice mutex here to prevent race conditions if user is rendering concurrently to multiple windows, each with its
    // own renderer, swap chain etcetera
    std::scoped_lock lock(Core::GetGraphicsMutex());
    return vkQueueSubmit(Core::GetGraphicsQueue(), 1, &submitInfo, info.SyncData[m_FrameIndex].InFlightFence);
}
VkResult FrameScheduler::Present() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &info.SyncData[m_FrameIndex].RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &m_ImageIndex;

    m_FrameIndex = (m_FrameIndex + 1) % VKIT_MAX_FRAMES_IN_FLIGHT;
    std::scoped_lock lock(Core::GetPresentMutex());
    return vkQueuePresentKHR(Core::GetPresentQueue(), &presentInfo);
}

u32 FrameScheduler::GetFrameIndex() const noexcept
{
    return m_FrameIndex;
}

VkRenderPass FrameScheduler::GetRenderPass() const noexcept
{
    return m_RenderPass;
}

VkCommandBuffer FrameScheduler::GetCurrentCommandBuffer() const noexcept
{
    return m_CommandBuffers[m_FrameIndex];
}

const VKit::SwapChain &FrameScheduler::GetSwapChain() const noexcept
{
    return m_SwapChain;
}

VkPresentModeKHR FrameScheduler::GetPresentMode() const noexcept
{
    return m_PresentMode;
}
void FrameScheduler::SetPresentMode(const VkPresentModeKHR p_PresentMode) noexcept
{
    m_PresentModeChanged = m_PresentMode != p_PresentMode;
    m_PresentMode = p_PresentMode;
}

void FrameScheduler::createSwapChain(Window &p_Window) noexcept
{
    VkExtent2D windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
        glfwWaitEvents();
    }
    Core::DeviceWaitIdle();

    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto result =
        VKit::SwapChain::Builder(&device, p_Window.GetSurface())
            .RequestSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .RequestPresentMode(m_PresentMode)
            .RequestExtent(windowExtent)
            .RequestDepthFormat(VK_FORMAT_D32_SFLOAT_S8_UINT)
            .SetOldSwapChain(m_SwapChain)
            .SetAllocator(Core::GetVulkanAllocator())
            .AddFlags(VKit::SwapChainBuilderFlags_Clipped | VKit::SwapChainBuilderFlags_CreateDefaultDepthResources |
                      VKit::SwapChainBuilderFlags_CreateImageViews |
                      VKit::SwapChainBuilderFlags_CreateDefaultSyncObjects)
            .Build();

    VKIT_ASSERT_RESULT(result);

    VKit::SwapChain old = m_SwapChain;
    m_SwapChain = result.GetValue();
    if (m_RenderPass)
        m_SwapChain.CreateDefaultFrameBuffers(m_RenderPass);
    if (old)
        old.Destroy();
}

void FrameScheduler::createRenderPass() noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = info.DepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = info.SurfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    TKIT_ASSERT_RETURNS(vkCreateRenderPass(Core::GetDevice(), &renderPassInfo, nullptr, &m_RenderPass), VK_SUCCESS,
                        "Failed to create render pass");

    m_SwapChain.CreateDefaultFrameBuffers(m_RenderPass);
}

void FrameScheduler::createCommandPool() noexcept
{
    VKit::CommandPool::Specs specs{};
    specs.QueueFamilyIndex = Core::GetDevice().GetPhysicalDevice().GetInfo().GraphicsIndex;
    specs.Flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    const auto result = VKit::CommandPool::Create(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    m_CommandPool = result.GetValue();
}

void FrameScheduler::createCommandBuffers() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = VKIT_MAX_FRAMES_IN_FLIGHT;

    TKIT_ASSERT_RETURNS(vkAllocateCommandBuffers(Core::GetDevice(), &allocInfo, m_CommandBuffers.data()), VK_SUCCESS,
                        "Failed to create command buffers");
}

} // namespace Onyx