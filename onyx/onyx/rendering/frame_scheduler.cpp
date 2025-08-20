#include "onyx/core/pch.hpp"
#include "onyx/rendering/frame_scheduler.hpp"
#include "onyx/app/window.hpp"
#include "onyx/property/color.hpp"
#include "onyx/core/shaders.hpp"
#include "tkit/utils/logging.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "vulkan/vulkan_core.h"

namespace Onyx
{
const VkFormat s_DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
FrameScheduler::FrameScheduler(Window &p_Window) noexcept
{
    createSwapChain(p_Window);
    m_InFlightImages.Resize(m_SwapChain.GetInfo().ImageData.GetSize(), VK_NULL_HANDLE);
    m_Images = createImageData();
    createProcessingEffects();
    createCommandData();
    const auto result = VKit::CreateSynchronizationObjects(Core::GetDevice(), m_SyncData);
    VKIT_ASSERT_RESULT(result);
}

FrameScheduler::~FrameScheduler() noexcept
{
    // Lock the queues to prevent any other command buffers from being submitted
    WaitIdle();
    if (m_PresentTask)
        Core::GetTaskManager()->DestroyTask(m_PresentTask);
    Core::DeviceWaitIdle();
    destroyImageData();
    m_PostProcessing.Destruct();
    m_NaivePostProcessingFragmentShader.Destroy();
    m_NaivePostProcessingLayout.Destroy();
    VKit::DestroySynchronizationObjects(Core::GetDevice(), m_SyncData);
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        m_CommandPools[i].Destroy();

    m_SwapChain.Destroy();
}

void FrameScheduler::handlePresentResult(Window &p_Window, const VkResult p_Result) noexcept
{
    if (p_Result == VK_ERROR_SURFACE_LOST_KHR)
        recreateSurface(p_Window);

    const bool resizeFixes = p_Result == VK_ERROR_OUT_OF_DATE_KHR || p_Result == VK_SUBOPTIMAL_KHR ||
                             p_Window.wasResized() || m_PresentModeChanged;

    TKIT_ASSERT(resizeFixes || p_Result == VK_SUCCESS || p_Result == VK_ERROR_SURFACE_LOST_KHR,
                "[ONYX] Failed to submit command buffers");

    if (resizeFixes)
    {
        recreateSwapChain(p_Window);
        p_Window.flagResizeDone();
        m_PresentModeChanged = false;
    }
}

VkCommandBuffer FrameScheduler::BeginFrame(Window &p_Window) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::BeginFrame");
    TKIT_ASSERT(!m_FrameStarted, "[ONYX] Cannot begin a new frame when there is already one in progress");

    const Window::Flags flags = p_Window.GetFlags();
    if ((flags & Window::Flag_ConcurrentQueueSubmission) && m_PresentTask)
    {
        TKit::ITaskManager *tm = Core::GetTaskManager();
        const VkResult result = tm->WaitForResult(m_PresentTask);
        handlePresentResult(p_Window, result);
    }

    const VkResult result = AcquireNextImage();
    if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        recreateSurface(p_Window);
        return VK_NULL_HANDLE;
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreateSwapChain(p_Window);
        return VK_NULL_HANDLE;
    }

    TKIT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "[ONYX] Failed to acquire swap chain image");
    m_FrameStarted = true;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const auto cmdres = m_CommandPools[m_FrameIndex].Reset();
    VKIT_ASSERT_RESULT(cmdres);

    const auto &table = Core::GetDeviceTable();
    TKIT_ASSERT_RETURNS(table.BeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &beginInfo), VK_SUCCESS,
                        "[ONYX] Failed to begin command buffer");

    return m_CommandBuffers[m_FrameIndex];
}

void FrameScheduler::EndFrame(Window &p_Window) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::EndFrame");
    TKIT_ASSERT(m_FrameStarted, "[ONYX] Cannot end a frame when there is no frame in progress");
    TKit::ITaskManager *tm = Core::GetTaskManager();
    const auto &table = Core::GetDeviceTable();
    TKIT_ASSERT_RETURNS(table.EndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS,
                        "[ONYX] Failed to end command buffer");

    const u32 frameIndex = m_FrameIndex;
    const u32 imageIndex = m_ImageIndex;

    const auto submission = [this, frameIndex, imageIndex]() {
        TKIT_ASSERT_RETURNS(SubmitCurrentCommandBuffer(frameIndex, imageIndex), VK_SUCCESS,
                            "[ONYX] Failed to submit command buffer");
        return Present(frameIndex, imageIndex);
    };

    const Window::Flags flags = p_Window.GetFlags();
    if (flags & Window::Flag_ConcurrentQueueSubmission)
    {
        if (m_PresentTask)
            tm->DestroyTask(m_PresentTask);
        m_PresentTask = tm->CreateAndSubmit(submission);
    }
    else
    {
        const VkResult result = submission();
        handlePresentResult(p_Window, result);
    }

    m_FrameStarted = false;
    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
}

static void transitionImage(const VKit::Vulkan::DeviceTable &p_Table, const VkCommandBuffer p_Cmd,
                            FrameScheduler::Image &p_Image, const VkImageLayout p_NewLayout,
                            const VkAccessFlags p_SrcAccess, const VkAccessFlags p_DstAccess,
                            const VkPipelineStageFlags p_SrcStage, const VkPipelineStageFlags p_DstStage,
                            const VkImageAspectFlags p_AspectMask = VK_IMAGE_ASPECT_COLOR_BIT) noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = p_Image.Layout;
    barrier.newLayout = p_NewLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = p_Image.Image.Image;
    barrier.subresourceRange = {p_AspectMask, 0, 1, 0, 1};
    barrier.srcAccessMask = p_SrcAccess;
    barrier.dstAccessMask = p_DstAccess;

    p_Table.CmdPipelineBarrier(p_Cmd, p_SrcStage, p_DstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    p_Image.Layout = p_NewLayout;
}

void FrameScheduler::BeginRendering(const Color &p_ClearColor) noexcept
{
    TKIT_ASSERT(m_FrameStarted, "[ONYX] Cannot begin rendering if a frame is not in progress");

    const auto &table = Core::GetDeviceTable();
    const VkCommandBuffer cmd = m_CommandBuffers[m_FrameIndex];

    VkRenderingAttachmentInfoKHR intermediateColor{};
    intermediateColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    intermediateColor.imageView = m_Images[m_ImageIndex].Intermediate.Image.ImageView;
    intermediateColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    intermediateColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    intermediateColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    intermediateColor.clearValue.color = {
        {p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};

    transitionImage(table, cmd, m_Images[m_ImageIndex].Intermediate, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = m_Images[m_ImageIndex].DepthStencil.Image.ImageView;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    transitionImage(table, cmd, m_Images[m_ImageIndex].DepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    const VkRenderingAttachmentInfoKHR stencil = depth;

    const VkExtent2D &extent = m_SwapChain.GetInfo().Extent;
    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &intermediateColor;
    renderInfo.pDepthAttachment = &depth;
    renderInfo.pStencilAttachment = &stencil;

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    table.CmdSetViewport(cmd, 0, 1, &viewport);
    table.CmdSetScissor(cmd, 0, 1, &scissor);

    table.CmdBeginRenderingKHR(cmd, &renderInfo);
}

void FrameScheduler::EndRendering() noexcept
{
    TKIT_ASSERT(m_FrameStarted, "[ONYX] Cannot end rendering if a frame is not in progress");
    const auto &table = Core::GetDeviceTable();

    const VkCommandBuffer cmd = m_CommandBuffers[m_FrameIndex];
    table.CmdEndRenderingKHR(cmd);

    VkRenderingAttachmentInfoKHR finalColor{};
    finalColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    finalColor.imageView = m_Images[m_ImageIndex].Presentation.Image.ImageView;
    finalColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    finalColor.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    finalColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    transitionImage(table, cmd, m_Images[m_ImageIndex].Presentation, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    transitionImage(table, cmd, m_Images[m_ImageIndex].Intermediate, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    const VkExtent2D &extent = m_SwapChain.GetInfo().Extent;
    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &finalColor;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    table.CmdBeginRenderingKHR(cmd, &renderInfo);

    m_PostProcessing->Bind(cmd, m_ImageIndex);
    m_PostProcessing->Draw(cmd);

    table.CmdEndRenderingKHR(cmd);

    transitionImage(table, cmd, m_Images[m_ImageIndex].Presentation, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

VkResult FrameScheduler::AcquireNextImage() noexcept
{
    const auto &table = Core::GetDeviceTable();
    TKIT_ASSERT_RETURNS(
        table.WaitForFences(Core::GetDevice(), 1, &m_SyncData[m_FrameIndex].InFlightFence, VK_TRUE, UINT64_MAX),
        VK_SUCCESS, "[ONYX] Failed to wait for fences");
    return table.AcquireNextImageKHR(Core::GetDevice(), m_SwapChain, UINT64_MAX,
                                     m_SyncData[m_FrameIndex].ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);
}
VkResult FrameScheduler::SubmitCurrentCommandBuffer(const u32 p_FrameIndex, const u32 p_ImageIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::SubmitCurrentCommandBuffer");
    const auto &table = Core::GetDeviceTable();
    const VkCommandBuffer cmd = m_CommandBuffers[p_FrameIndex];

    if (m_InFlightImages[m_ImageIndex] != VK_NULL_HANDLE)
    {
        TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::WaitForPreviousFrame");
        TKIT_ASSERT_RETURNS(
            table.WaitForFences(Core::GetDevice(), 1, &m_InFlightImages[p_ImageIndex], VK_TRUE, UINT64_MAX), VK_SUCCESS,
            "[ONYX] Failed to wait for fences");
    }

    m_InFlightImages[p_ImageIndex] = m_SyncData[p_FrameIndex].InFlightFence;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_SyncData[p_FrameIndex].ImageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_SyncData[p_FrameIndex].RenderFinishedSemaphore;

    TKIT_ASSERT_RETURNS(table.ResetFences(Core::GetDevice(), 1, &m_SyncData[p_FrameIndex].InFlightFence), VK_SUCCESS,
                        "[ONYX] Failed to reset fences");
    return table.QueueSubmit(Core::GetGraphicsQueue(), 1, &submitInfo, m_SyncData[p_FrameIndex].InFlightFence);
}
VkResult FrameScheduler::Present(const u32 p_FrameIndex, const u32 p_ImageIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_SyncData[p_FrameIndex].RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &p_ImageIndex;

    const auto &table = Core::GetDeviceTable();
    return table.QueuePresentKHR(Core::GetPresentQueue(), &presentInfo);
}

PostProcessing *FrameScheduler::SetPostProcessing(const VKit::PipelineLayout &p_Layout,
                                                  const VKit::Shader &p_FragmentShader,
                                                  const PostProcessingOptions &p_Options) noexcept
{
    PostProcessing::Specs specs{};
    specs.Layout = p_Layout;
    specs.FragmentShader = p_FragmentShader;
    specs.VertexShader = p_Options.VertexShader ? *p_Options.VertexShader : GetFullPassVertexShader();
    specs.SamplerCreateInfo =
        p_Options.SamplerCreateInfo ? *p_Options.SamplerCreateInfo : PostProcessing::DefaultSamplerCreateInfo();
    specs.RenderInfo = CreatePostProcessingRenderInfo();
    m_PostProcessing->Setup(specs);
    return m_PostProcessing.Get();
}

PostProcessing *FrameScheduler::GetPostProcessing() noexcept
{
    return m_PostProcessing.Get();
}

void FrameScheduler::WaitIdle() const noexcept
{
    if (m_PresentTask)
        Core::GetTaskManager()->WaitUntilFinished(m_PresentTask);
}

void FrameScheduler::RemovePostProcessing() noexcept
{
    setupNaivePostProcessing();
}

u32 FrameScheduler::GetFrameIndex() const noexcept
{
    return m_FrameIndex;
}
VkPipelineRenderingCreateInfoKHR FrameScheduler::CreateSceneRenderInfo() const noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    VkPipelineRenderingCreateInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &info.SurfaceFormat.format;
    renderInfo.depthAttachmentFormat = s_DepthStencilFormat;
    renderInfo.stencilAttachmentFormat = s_DepthStencilFormat;
    return renderInfo;
}
VkPipelineRenderingCreateInfoKHR FrameScheduler::CreatePostProcessingRenderInfo() const noexcept
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    VkPipelineRenderingCreateInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &info.SurfaceFormat.format;
    renderInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    return renderInfo;
}

const VKit::SwapChain &FrameScheduler::GetSwapChain() const noexcept
{
    return m_SwapChain;
}

VkCommandBuffer FrameScheduler::GetCurrentCommandBuffer() const noexcept
{
    return m_CommandBuffers[m_FrameIndex];
}

VkPresentModeKHR FrameScheduler::GetPresentMode() const noexcept
{
    return m_PresentMode;
}
TKit::StaticArray8<VkPresentModeKHR> FrameScheduler::GetAvailablePresentModes() const noexcept
{

    return m_SwapChain.GetInfo().SupportDetails.PresentModes;
}
void FrameScheduler::SetPresentMode(const VkPresentModeKHR p_PresentMode) noexcept
{
    if (m_PresentMode == p_PresentMode)
        return;
    m_PresentModeChanged = true;
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
    TKIT_LOG_INFO("[ONYX] Out of date swap chain. Re-creating swap chain and resources");
    VKit::SwapChain old = m_SwapChain;
    createSwapChain(p_Window);
    old.Destroy();
    recreateResources();
}
void FrameScheduler::recreateSurface(Window &p_Window) noexcept
{
    TKIT_LOG_INFO("[ONYX] Surface lost... re-creating surface, swap chain and resources");
    Core::DeviceWaitIdle();

    m_SwapChain.Destroy();
    m_SwapChain = VKit::SwapChain{};

    p_Window.recreateSurface();
    createSwapChain(p_Window);
    recreateResources();
}

void FrameScheduler::recreateResources() noexcept
{
    destroyImageData();
    m_Images = createImageData();
    m_InFlightImages.Resize(m_SwapChain.GetInfo().ImageData.GetSize(), VK_NULL_HANDLE);

    const TKit::StaticArray4<VkImageView> imageViews = getIntermediateColorImageViews();
    m_PostProcessing->updateImageViews(imageViews);
    m_ImageIndex = 0;
}

TKit::StaticArray4<FrameScheduler::ImageData> FrameScheduler::createImageData() noexcept
{
    const auto result = VKit::ImageHouse::Create(Core::GetDevice(), Core::GetVulkanAllocator());
    VKIT_ASSERT_RESULT(result);
    m_ImageHouse = result.GetValue();

    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    TKit::StaticArray4<ImageData> images{};
    for (u32 i = 0; i < info.ImageData.GetSize(); ++i)
    {
        ImageData data{};
        auto iresult = m_ImageHouse.CreateImage(info.ImageData[i].Image, info.ImageData[i].ImageView);
        VKIT_ASSERT_RESULT(iresult);
        data.Presentation.Image = iresult.GetValue();
        data.Presentation.Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        iresult = m_ImageHouse.CreateImage(info.SurfaceFormat.format, info.Extent,
                                           VKit::AttachmentFlag_Color | VKit::AttachmentFlag_Sampled);
        VKIT_ASSERT_RESULT(iresult);
        data.Intermediate.Image = iresult.GetValue();
        data.Intermediate.Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        iresult = m_ImageHouse.CreateImage(s_DepthStencilFormat, info.Extent,
                                           VKit::AttachmentFlag_Depth | VKit::AttachmentFlag_Stencil);
        VKIT_ASSERT_RESULT(iresult);
        data.DepthStencil.Image = iresult.GetValue();
        data.DepthStencil.Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        images.Append(data);
    }
    return images;
}
void FrameScheduler::destroyImageData() noexcept
{
    for (const ImageData &data : m_Images)
    {
        m_ImageHouse.DestroyImage(data.Presentation.Image);
        m_ImageHouse.DestroyImage(data.Intermediate.Image);
        m_ImageHouse.DestroyImage(data.DepthStencil.Image);
    }
}

void FrameScheduler::createProcessingEffects() noexcept
{
    m_NaivePostProcessingFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/pp-naive.frag");

    const TKit::StaticArray4<VkImageView> imageviews = getIntermediateColorImageViews();
    m_PostProcessing.Construct(imageviews);

    const VKit::PipelineLayout::Builder builder = m_PostProcessing->CreatePipelineLayoutBuilder();
    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    m_NaivePostProcessingLayout = result.GetValue();

    setupNaivePostProcessing();
}

void FrameScheduler::createCommandData() noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const auto result =
            VKit::CommandPool::Create(Core::GetDevice(), Core::GetDevice().GetPhysicalDevice().GetInfo().GraphicsIndex,
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

        VKIT_ASSERT_RESULT(result);
        m_CommandPools[i] = result.GetValue();

        const auto cmdres = m_CommandPools[i].Allocate();
        VKIT_ASSERT_RESULT(cmdres);
        m_CommandBuffers[i] = cmdres.GetValue();
    }
}

void FrameScheduler::setupNaivePostProcessing() noexcept
{
    PostProcessing::Specs specs{};
    specs.Layout = m_NaivePostProcessingLayout;
    specs.FragmentShader = m_NaivePostProcessingFragmentShader;
    specs.VertexShader = GetFullPassVertexShader();
    specs.SamplerCreateInfo = PostProcessing::DefaultSamplerCreateInfo();
    specs.RenderInfo = CreatePostProcessingRenderInfo();
    m_PostProcessing->Setup(specs);
}

TKit::StaticArray4<VkImageView> FrameScheduler::getIntermediateColorImageViews() const noexcept
{
    TKit::StaticArray4<VkImageView> imageViews;
    for (u32 i = 0; i < m_SwapChain.GetInfo().ImageData.GetSize(); ++i)
        imageViews.Append(m_Images[i].Intermediate.Image.ImageView);
    return imageViews;
}

} // namespace Onyx
