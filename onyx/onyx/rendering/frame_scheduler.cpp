#include "onyx/core/pch.hpp"
#include "onyx/rendering/frame_scheduler.hpp"
#include "onyx/app/window.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/core/shaders.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

#include "onyx/core/glfw.hpp"

namespace Onyx
{
FrameScheduler::FrameScheduler(Window &p_Window) noexcept
{
    createSwapChain(p_Window);
    m_InFlightImages.resize(m_SwapChain.GetInfo().ImageData.size(), VK_NULL_HANDLE);
    createRenderPass();
    createProcessingEffects();
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
    m_PreProcessing.Destroy();
    m_PostProcessing.Destroy();
    m_ProcessingEffectVertexShader.Destroy();
    m_NaivePostProcessingFragmentShader.Destroy();
    m_NaivePostProcessingLayout.Destroy();
    VKit::DestroySynchronizationObjects(Core::GetDevice(), m_SyncData);
    vkFreeCommandBuffers(Core::GetDevice(), m_CommandPool, ONYX_MAX_FRAMES_IN_FLIGHT, m_CommandBuffers.data());
    vkDestroyCommandPool(Core::GetDevice(), m_CommandPool, nullptr);
    vkDestroyRenderPass(Core::GetDevice(), m_RenderPass, nullptr);
    m_SwapChain.Destroy();
}

VkCommandBuffer FrameScheduler::BeginFrame(Window &p_Window) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::BeginFrame");
    TKIT_ASSERT(!checkFlag(Flag_FrameStarted), "Cannot begin a new frame when there is already one in progress");
    if (m_PresentTask) [[likely]]
    {
        const VkResult result = m_PresentTask->WaitForResult();
        const bool resizeFixes = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                                 p_Window.WasResized() || checkFlag(Flag_PresentModeChanged);

        TKIT_ASSERT(resizeFixes || result == VK_SUCCESS, "Failed to submit command buffers");
        if (resizeFixes)
        {
            recreateSwapChain(p_Window);
            p_Window.FlagResizeDone();
            m_Flags &= ~Flag_PresentModeChanged;
            return VK_NULL_HANDLE;
        }
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
    m_Flags |= Flag_FrameStarted;

    if (checkFlag(Flag_SignalSetupPreProcessing)) [[unlikely]]
    {
        m_PreProcessing->Setup(m_PreProcessingSpecs);
        m_Flags &= ~Flag_SignalSetupPreProcessing;
    }
    if (checkFlag(Flag_SignalSetupPostProcessing)) [[unlikely]]
    {
        m_PostProcessing->Setup(m_PostProcessingSpecs);
        m_Flags &= ~Flag_SignalSetupPostProcessing;
    }
    if (checkFlag(Flag_SignalRemovePreProcessing)) [[unlikely]]
    {
        m_PreProcessing.Destroy();
        m_PreProcessing.Create(m_RenderPass, m_ProcessingEffectVertexShader);
        m_Flags &= ~Flag_SignalRemovePreProcessing;
    }
    if (checkFlag(Flag_SignalRemovePostProcessing)) [[unlikely]]
    {
        setupNaivePostProcessing();
        m_Flags &= ~Flag_SignalRemovePostProcessing;
    }

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
    TKIT_ASSERT(checkFlag(Flag_FrameStarted), "Cannot end a frame when there is no frame in progress");
    TKIT_ASSERT_RETURNS(vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS, "Failed to end command buffer");

    TKit::ITaskManager *taskManager = Core::GetTaskManager();

    m_PresentTask->Reset();
    taskManager->SubmitTask(m_PresentTask);
    m_Flags &= ~Flag_FrameStarted;
}

void FrameScheduler::BeginRenderPass(const Color &p_ClearColor) noexcept
{
    TKIT_ASSERT(checkFlag(Flag_FrameStarted), "Cannot begin render pass if a frame is not in progress");

    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const VkExtent2D extent = info.Extent;

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_RenderPass;
    passInfo.framebuffer = m_Resources.GetFrameBuffer(m_ImageIndex);
    passInfo.renderArea.offset = {0, 0};
    passInfo.renderArea.extent = extent;

    std::array<VkClearValue, 3> clear_values;
    clear_values[0].color = {{p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};
    clear_values[1].depthStencil.depth = 1.f;
    clear_values[1].depthStencil.stencil = 0;
    clear_values[2].color = {{p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};

    passInfo.clearValueCount = static_cast<u32>(clear_values.size());
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

    if (*m_PreProcessing)
    {
        m_PreProcessing->Bind(m_FrameIndex, m_CommandBuffers[m_FrameIndex]);
        m_PreProcessing->Draw(m_CommandBuffers[m_FrameIndex]);
    }
    vkCmdNextSubpass(m_CommandBuffers[m_FrameIndex], VK_SUBPASS_CONTENTS_INLINE);
}

void FrameScheduler::EndRenderPass() noexcept
{
    TKIT_ASSERT(checkFlag(Flag_FrameStarted), "Cannot end render pass if a frame is not in progress");
    vkCmdNextSubpass(m_CommandBuffers[m_FrameIndex], VK_SUBPASS_CONTENTS_INLINE);
    m_PostProcessing->Bind(m_FrameIndex, m_ImageIndex, m_CommandBuffers[m_FrameIndex]);
    m_PostProcessing->Draw(m_CommandBuffers[m_FrameIndex]);

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

PreProcessing *FrameScheduler::SetupPreProcessing(const VKit::PipelineLayout &p_Layout,
                                                  const VKit::Shader &p_FragmentShader) noexcept
{
    m_PreProcessingSpecs = {p_Layout, p_FragmentShader};
    m_Flags |= Flag_SignalSetupPreProcessing;
    m_PreProcessing->ResizeResourceContainers(p_Layout.GetInfo());
    return m_PreProcessing.Get();
}
PostProcessing *FrameScheduler::SetupPostProcessing(const VKit::PipelineLayout &p_Layout,
                                                    const VKit::Shader &p_FragmentShader,
                                                    const VkSamplerCreateInfo *p_Info) noexcept
{
    m_PostProcessingSpecs = {p_Layout, p_FragmentShader, p_Info ? *p_Info : PostProcessing::DefaultSamplerCreateInfo()};
    m_Flags |= Flag_SignalSetupPostProcessing;
    m_PostProcessing->ResizeResourceContainers(p_Layout.GetInfo());
    return m_PostProcessing.Get();
}

PreProcessing *FrameScheduler::GetPreProcessing() noexcept
{
    return m_PreProcessing.Get();
}
PostProcessing *FrameScheduler::GetPostProcessing() noexcept
{
    return m_PostProcessing.Get();
}

void FrameScheduler::RemovePreProcessing() noexcept
{
    m_Flags |= Flag_SignalRemovePreProcessing;
}
void FrameScheduler::RemovePostProcessing() noexcept
{
    m_Flags |= Flag_SignalRemovePostProcessing;
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
    if (m_PresentMode == p_PresentMode)
        return;
    m_Flags |= Flag_PresentModeChanged;
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

    const TKit::StaticArray4<VkImageView> imageViews = getIntermediateAttachmentImageViews();
    m_PostProcessing->UpdateImageViews(imageViews);
}

void FrameScheduler::createRenderPass() noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    const u32 imageCount = static_cast<u32>(info.ImageData.size());

    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto result =
        VKit::RenderPass::Builder(&device, imageCount)
            .SetAllocator(Core::GetVulkanAllocator())
            // Attachment 0: This is the final presentation image. It is the post processing target image.
            .BeginAttachment(VKit::Attachment::Flag_Color)
            .RequestFormat(info.SurfaceFormat.format)
            .SetFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .EndAttachment()
            // Attachment 1: Depth/Stencil. This is the main depth/stencil buffer for the scene.
            .BeginAttachment(VKit::Attachment::Flag_Depth | VKit::Attachment::Flag_Stencil)
            .RequestFormat(VK_FORMAT_D32_SFLOAT_S8_UINT)
            .SetFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .EndAttachment()
            // Attachment 2: An intermediate color attachment used as the target of the pre-processing pass and the main
            // scene itself. It will also serve as "input" for the post-processing pass. This attachment is supplied to
            // the post processing pipeline via a sampler, so in theory, flagging it as an input attachment would not be
            // necessary. However, it is flagged as an input attachment to ensure that vulkan is
            // aware of the dependency between the scene rendering and the post-processing pass. It is kind of a quirk
            // that allows us to defer synchronization to the render pass itself.
            .BeginAttachment(VKit::Attachment::Flag_Color | VKit::Attachment::Flag_Sampled |
                             VKit::Attachment::Flag_Input)
            .RequestFormat(info.SurfaceFormat.format)
            .SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .EndAttachment()
            // Subpass 0 - Runs the pre-processing pass
            .BeginSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .AddColorAttachment(2)
            .EndSubpass()
            // Subpass 1 - Runs the main scene rendering pass
            .BeginSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .AddColorAttachment(2)
            .SetDepthStencilAttachment(1)
            .EndSubpass()
            // Subpass 2 - Runs the post-processing pass
            .BeginSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .AddColorAttachment(0)
            .AddInputAttachment(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .EndSubpass()
            // Dependency 0 - External to pre-processing
            .BeginDependency(VK_SUBPASS_EXTERNAL, 0)
            .SetStageMask(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
            .SetAccessMask(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .EndDependency()
            // Dependency 1 - Pre-processing to scene rendering. Here, we tell vulkan that subpass 1 will write to
            // attachment 2 and need the pre-processing pass to be finished before that.
            .BeginDependency(0, 1)
            .SetStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
            .SetAccessMask(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .EndDependency()
            // Dependency 2 - Scene rendering to post-processing. Here, we tell vulkan that subpass 2 will read from
            // attachment 2 from the fragment shader and need the scene rendering pass to be finished before that.
            .BeginDependency(1, 2)
            .SetStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .SetAccessMask(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
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

void FrameScheduler::createProcessingEffects() noexcept
{
    m_ProcessingEffectVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/full-pass.vert");
    m_NaivePostProcessingFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/naive-post-processing.frag");

    const TKit::StaticArray4<VkImageView> imageviews = getIntermediateAttachmentImageViews();
    m_PreProcessing.Create(m_RenderPass, m_ProcessingEffectVertexShader);
    m_PostProcessing.Create(m_RenderPass, m_ProcessingEffectVertexShader, imageviews);

    const VKit::PipelineLayout::Builder builder = m_PostProcessing->CreatePipelineLayoutBuilder();
    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    m_NaivePostProcessingLayout = result.GetValue();

    setupNaivePostProcessing();
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

void FrameScheduler::setupNaivePostProcessing() noexcept
{
    PostProcessing::Specs specs{};
    specs.Layout = m_NaivePostProcessingLayout;
    specs.FragmentShader = m_NaivePostProcessingFragmentShader;
    m_PostProcessing->ResizeResourceContainers(specs.Layout.GetInfo());
    m_PostProcessing->Setup(specs);
}

TKit::StaticArray4<VkImageView> FrameScheduler::getIntermediateAttachmentImageViews() const noexcept
{
    TKit::StaticArray4<VkImageView> imageViews;
    const u32 imageCount = static_cast<u32>(m_SwapChain.GetInfo().ImageData.size());
    for (u32 i = 0; i < imageCount; ++i)
        imageViews.push_back(m_Resources.GetImageView(i, 2));
    return imageViews;
}

bool FrameScheduler::checkFlag(Flags p_Flag) const noexcept
{
    return m_Flags & p_Flag;
}

} // namespace Onyx