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
    const auto result = VKit::CreateSynchronizationObjects(Core::GetDevice(), m_SyncData);
    VKIT_ASSERT_VULKAN_RESULT(result);
}

FrameScheduler::~FrameScheduler() noexcept
{
    if (m_PresentTask)
        m_PresentTask->WaitUntilFinished();
    // Must wait for the device. Windows/RenderContexts may be destroyed at runtime, and all its command buffers must
    // have finished

    // Lock the queues to prevent any other command buffers from being submitted
    Core::DeviceWaitIdle();
    m_Resources.Destroy();
    VKit::DestroySynchronizationObjects(Core::GetDevice(), m_SyncData);
    vkFreeCommandBuffers(Core::GetDevice(), m_CommandPool, ONYX_MAX_FRAMES_IN_FLIGHT, m_CommandBuffers.data());
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
            recreateSwapChain(p_Window);
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
        recreateSwapChain(p_Window);
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
    passInfo.framebuffer = m_Resources.GetFrameBuffer(m_ImageIndex);
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
    vkWaitForFences(Core::GetDevice(), 1, &m_SyncData[m_FrameIndex].InFlightFence, VK_TRUE, UINT64_MAX);
    return vkAcquireNextImageKHR(Core::GetDevice(), m_SwapChain, UINT64_MAX,
                                 m_SyncData[m_FrameIndex].ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);
}
VkResult FrameScheduler::SubmitCurrentCommandBuffer() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::SubmitCurrentCommandBuffer");
    const VkCommandBuffer cmd = m_CommandBuffers[m_FrameIndex];

    if (m_InFlightImages[m_ImageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(Core::GetDevice(), 1, &m_InFlightImages[m_ImageIndex], VK_TRUE, UINT64_MAX);

    m_InFlightImages[m_ImageIndex] = m_SyncData[m_FrameIndex].InFlightFence;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_SyncData[m_FrameIndex].ImageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_SyncData[m_FrameIndex].RenderFinishedSemaphore;

    vkResetFences(Core::GetDevice(), 1, &m_SyncData[m_FrameIndex].InFlightFence);

    // A nice mutex here to prevent race conditions if user is rendering concurrently to multiple windows, each with its
    // own renderer, swap chain etcetera
    std::scoped_lock lock(Core::GetGraphicsMutex());
    return vkQueueSubmit(Core::GetGraphicsQueue(), 1, &submitInfo, m_SyncData[m_FrameIndex].InFlightFence);
}
VkResult FrameScheduler::Present() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_SyncData[m_FrameIndex].RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &m_ImageIndex;

    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
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
            .SetOldSwapChain(m_SwapChain)
            .AddFlags(VKit::SwapChain::Builder::Flag_Clipped | VKit::SwapChain::Builder::Flag_CreateImageViews)
            .Build();

    VKIT_ASSERT_RESULT(result);
    m_SwapChain = result.GetValue();
}

void FrameScheduler::recreateSwapChain(Window &p_Window) noexcept
{
    VKit::SwapChain old = m_SwapChain;
    createSwapChain(p_Window);
    old.Destroy();

    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const auto result =
        m_RenderPass.CreateResources(info.Extent, [this, &info](const u32 p_ImageIndex, const u32 p_AttachmentIndex) {
            return p_AttachmentIndex == 0 ? m_RenderPass.CreateImageData(info.ImageData[p_ImageIndex].ImageView)
                                          : m_RenderPass.CreateImageData(p_AttachmentIndex, info.Extent);
        });
    VKIT_ASSERT_RESULT(result);
    m_Resources.Destroy();
    m_Resources = result.GetValue();
}

void FrameScheduler::createRenderPass() noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const u32 imageCount = static_cast<u32>(info.ImageData.size());

    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto result =
        VKit::RenderPass::Builder(&device, imageCount)
            .SetAllocator(Core::GetVulkanAllocator())
            // Attachment 0: Color
            .BeginAttachment(VKit::Attachment::Flag_Color)
            .RequestFormat(info.SurfaceFormat.format)
            .SetFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .EndAttachment()
            // Attachment 1: Depth/Stencil
            .BeginAttachment(VKit::Attachment::Flag_Depth | VKit::Attachment::Flag_Stencil)
            .RequestFormat(VK_FORMAT_D32_SFLOAT_S8_UINT)
            .SetFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .EndAttachment()
            // Subpass 0
            .BeginSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .AddColorAttachment(0)
            .SetDepthStencilAttachment(1)
            .EndSubpass()
            // Dependency 0
            .BeginDependency(VK_SUBPASS_EXTERNAL, 0)
            .SetAccessMask(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .SetStageMask(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
            .EndDependency()
            .Build();

    VKIT_ASSERT_RESULT(result);
    m_RenderPass = result.GetValue();
    const auto resresult =
        m_RenderPass.CreateResources(info.Extent, [this, &info](const u32 p_ImageIndex, const u32 p_AttachmentIndex) {
            return p_AttachmentIndex == 0 ? m_RenderPass.CreateImageData(info.ImageData[p_ImageIndex].ImageView)
                                          : m_RenderPass.CreateImageData(p_AttachmentIndex, info.Extent);
        });
    VKIT_ASSERT_RESULT(resresult);

    m_Resources = resresult.GetValue();
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
    allocInfo.commandBufferCount = ONYX_MAX_FRAMES_IN_FLIGHT;

    TKIT_ASSERT_RETURNS(vkAllocateCommandBuffers(Core::GetDevice(), &allocInfo, m_CommandBuffers.data()), VK_SUCCESS,
                        "Failed to create command buffers");
}

} // namespace Onyx