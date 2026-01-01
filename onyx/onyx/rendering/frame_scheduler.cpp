#include "onyx/core/pch.hpp"
#include "onyx/rendering/frame_scheduler.hpp"
#include "onyx/app/window.hpp"
#include "onyx/property/color.hpp"
#include "onyx/state/pipelines.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/profiling/macros.hpp"
#include "onyx/core/glfw.hpp"

namespace Onyx
{
static const VkFormat s_DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
const WaitMode WaitMode::Block = WaitMode{TKIT_U64_MAX, TKIT_U64_MAX};
const WaitMode WaitMode::Poll = WaitMode{0, 0};

FrameScheduler::FrameScheduler(Window &p_Window)
{
    m_QueueData = Detail::BorrowQueueData();

    const VkExtent2D extent = waitGlfwEvents(p_Window);
    createSwapChain(p_Window, extent);
    m_SyncImageData = Detail::CreatePerImageSyncData(m_SwapChain.GetImageCount());
    m_Images = createImageData();
    createProcessingEffects();
    createCommandData();
    m_SyncFrameData = Detail::CreatePerFrameSyncData();
}

FrameScheduler::~FrameScheduler()
{
    Core::DeviceWaitIdle();
    destroyImageData();
    m_PostProcessing.Destruct();
    m_NaivePostProcessingFragmentShader.Destroy();
    m_NaivePostProcessingLayout.Destroy();
    Detail::DestroyPerFrameSyncData(m_SyncFrameData);
    Detail::DestroyPerImageSyncData(m_SyncImageData);
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
        m_CommandData[i].GraphicsPool.Destroy();
        if (m_TransferMode == Transfer_Separate)
            m_CommandData[i].TransferPool.Destroy();
    }

    m_SwapChain.Destroy();
    Detail::ReturnQueueData(m_QueueData);
}

bool FrameScheduler::handleImageResult(Window &p_Window, const VkResult p_Result)
{
    if (p_Result == VK_NOT_READY || p_Result == VK_TIMEOUT)
        return false;

    if (p_Result == VK_ERROR_SURFACE_LOST_KHR)
    {
        recreateSurface(p_Window);
        return false;
    }

    const bool needRecreation =
        p_Result == VK_ERROR_OUT_OF_DATE_KHR || p_Result == VK_SUBOPTIMAL_KHR || m_RequestSwapchainRecreation;

    TKIT_ASSERT(needRecreation || p_Result == VK_SUCCESS || p_Result == VK_ERROR_SURFACE_LOST_KHR,
                "[ONYX] Failed to submit command buffers. The vulkan error is the following: {}",
                VKit::ResultToString(p_Result));

    if (needRecreation)
    {
        recreateSwapChain(p_Window);
        return false;
    }
    return true;
}

VkCommandBuffer FrameScheduler::BeginFrame(Window &p_Window, const WaitMode &p_WaitMode)
{
    const VkResult result = AcquireNextImage(p_WaitMode);
    if (!handleImageResult(p_Window, result))
        return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const CommandData &cmd = m_CommandData[m_FrameIndex];
    auto cmdres = cmd.GraphicsPool.Reset();
    VKIT_ASSERT_RESULT(cmdres);
    if (m_TransferMode == Transfer_Separate)
    {
        cmdres = cmd.TransferPool.Reset();
        VKIT_ASSERT_RESULT(cmdres);
    }

    const auto &table = Core::GetDeviceTable();
    VKIT_ASSERT_EXPRESSION(table.BeginCommandBuffer(cmd.GraphicsCommand, &beginInfo));
    if (m_TransferMode == Transfer_Separate)
    {
        VKIT_ASSERT_EXPRESSION(table.BeginCommandBuffer(cmd.TransferCommand, &beginInfo));
    }

    return cmd.GraphicsCommand;
}

void FrameScheduler::SubmitGraphicsQueue(const VkPipelineStageFlags p_Flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::SubmitCurrentCommandBuffer");
    const auto &table = Core::GetDeviceTable();

    const VkCommandBuffer cmd = m_CommandData[m_FrameIndex].GraphicsCommand;

    const Detail::SyncFrameData &fsync = m_SyncFrameData[m_FrameIndex];
    Detail::SyncImageData &isync = m_SyncImageData[m_ImageIndex];

    if (isync.InFlightImage != VK_NULL_HANDLE)
    {
        TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::WaitForImage");
        VKIT_ASSERT_EXPRESSION(table.WaitForFences(Core::GetDevice(), 1, &isync.InFlightImage, VK_TRUE, UINT64_MAX));
    }

    isync.InFlightImage = fsync.InFlightFence;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const TKit::FixedArray<VkSemaphore, 2> semaphores{fsync.ImageAvailableSemaphore, fsync.TransferCopyDoneSemaphore};
    const TKit::FixedArray<VkPipelineStageFlags, 2> stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, p_Flags};
    submitInfo.pWaitSemaphores = semaphores.GetData();
    submitInfo.pWaitDstStageMask = stages.GetData();
    submitInfo.waitSemaphoreCount = (m_TransferMode == Transfer_Separate && p_Flags != 0) ? 2 : 1;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &isync.RenderFinishedSemaphore;

    VKIT_ASSERT_EXPRESSION(table.ResetFences(Core::GetDevice(), 1, &fsync.InFlightFence));
    VKIT_ASSERT_EXPRESSION(table.QueueSubmit(m_QueueData.Graphics->Queue, 1, &submitInfo, fsync.InFlightFence));
}
VkResult FrameScheduler::Present()
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    const Detail::SyncImageData &sync = m_SyncImageData[m_ImageIndex];
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &sync.RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &m_ImageIndex;

    const auto &table = Core::GetDeviceTable();
    return table.QueuePresentKHR(m_QueueData.Present->Queue, &presentInfo);
}
void FrameScheduler::EndFrame(Window &p_Window, const VkPipelineStageFlags p_Flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::EndFrame");

    const VkCommandBuffer cmd = m_CommandData[m_FrameIndex].GraphicsCommand;
    const auto &table = Core::GetDeviceTable();
    VKIT_ASSERT_EXPRESSION(table.EndCommandBuffer(cmd));

    SubmitGraphicsQueue(p_Flags);
    const VkResult result = Present();

    handleImageResult(p_Window, result);

    m_FrameIndex = (m_FrameIndex + 1) % MaxFramesInFlight;
}

void FrameScheduler::BeginRendering(const Color &p_ClearColor)
{
    const auto &table = Core::GetDeviceTable();
    const VkCommandBuffer cmd = m_CommandData[m_FrameIndex].GraphicsCommand;

    VkRenderingAttachmentInfoKHR intermediateColor{};
    intermediateColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    intermediateColor.imageView = m_Images[m_ImageIndex].Intermediate.GetImageView();
    intermediateColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    intermediateColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    intermediateColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    intermediateColor.clearValue.color = {
        {p_ClearColor.RGBA[0], p_ClearColor.RGBA[1], p_ClearColor.RGBA[2], p_ClearColor.RGBA[3]}};

    m_Images[m_ImageIndex].Intermediate.TransitionLayout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                         {.SrcAccess = 0,
                                                          .DstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          .SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                          .DstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = m_Images[m_ImageIndex].DepthStencil.GetImageView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    m_Images[m_ImageIndex].DepthStencil.TransitionLayout(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                         {.SrcAccess = 0,
                                                          .DstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                          .SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                          .DstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT});

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

void FrameScheduler::EndRendering()
{
    const auto &table = Core::GetDeviceTable();

    const VkCommandBuffer cmd = m_CommandData[m_FrameIndex].GraphicsCommand;
    table.CmdEndRenderingKHR(cmd);

    VkRenderingAttachmentInfoKHR finalColor{};
    finalColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    finalColor.imageView = m_Images[m_ImageIndex].Presentation->GetImageView();
    finalColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    finalColor.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    finalColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    m_Images[m_ImageIndex].Presentation->TransitionLayout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                          {.SrcAccess = 0,
                                                           .DstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                           .SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                           .DstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});

    m_Images[m_ImageIndex].Intermediate.TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                         {.SrcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          .DstAccess = VK_ACCESS_SHADER_READ_BIT,
                                                          .SrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                          .DstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT});

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

    m_Images[m_ImageIndex].Presentation->TransitionLayout(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                          {.SrcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                           .DstAccess = 0,
                                                           .SrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                           .DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT});
}

VkResult FrameScheduler::AcquireNextImage(const WaitMode &p_WaitMode)
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::AcquireNextImage");
    const auto &table = Core::GetDeviceTable();
    {
        TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::WaitForFrame");
        const VkResult result = table.WaitForFences(Core::GetDevice(), 1, &m_SyncFrameData[m_FrameIndex].InFlightFence,
                                                    VK_TRUE, p_WaitMode.WaitFenceTimeout);
        if (result == VK_NOT_READY || result == VK_TIMEOUT)
            return result;
        VKIT_ASSERT_RESULT(result);
    }
    return table.AcquireNextImageKHR(Core::GetDevice(), m_SwapChain, p_WaitMode.AcquireTimeout,
                                     m_SyncFrameData[m_FrameIndex].ImageAvailableSemaphore, VK_NULL_HANDLE,
                                     &m_ImageIndex);
}

void FrameScheduler::SubmitTransferQueue()
{
    const auto &table = Core::GetDeviceTable();
    VKIT_ASSERT_EXPRESSION(table.EndCommandBuffer(m_CommandData[m_FrameIndex].TransferCommand));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandData[m_FrameIndex].TransferCommand;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_SyncFrameData[m_FrameIndex].TransferCopyDoneSemaphore;

    VKIT_ASSERT_EXPRESSION(table.QueueSubmit(m_QueueData.Transfer->Queue, 1, &submitInfo, VK_NULL_HANDLE));
}

PostProcessing *FrameScheduler::SetPostProcessing(const VKit::PipelineLayout &p_Layout,
                                                  const VKit::Shader &p_FragmentShader,
                                                  const PostProcessingOptions &p_Options)
{
    PostProcessing::Specs specs{};
    specs.Layout = p_Layout;
    specs.FragmentShader = p_FragmentShader;
    specs.VertexShader = p_Options.VertexShader ? *p_Options.VertexShader : Pipelines::GetFullPassVertexShader();
    specs.SamplerCreateInfo =
        p_Options.SamplerCreateInfo ? *p_Options.SamplerCreateInfo : PostProcessing::DefaultSamplerCreateInfo();
    specs.RenderInfo = CreatePostProcessingRenderInfo();
    m_PostProcessing->Setup(specs);
    return m_PostProcessing.Get();
}

PostProcessing *FrameScheduler::GetPostProcessing()
{
    return m_PostProcessing.Get();
}

void FrameScheduler::RemovePostProcessing()
{
    setupNaivePostProcessing();
}

VkPipelineRenderingCreateInfoKHR FrameScheduler::CreateSceneRenderInfo() const
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
VkPipelineRenderingCreateInfoKHR FrameScheduler::CreatePostProcessingRenderInfo() const
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

VkExtent2D FrameScheduler::waitGlfwEvents(Window &p_Window)
{
    VkExtent2D windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {p_Window.GetScreenWidth(), p_Window.GetScreenHeight()};
        glfwWaitEvents();
    }
    return windowExtent;
}
void FrameScheduler::createSwapChain(Window &p_Window, const VkExtent2D &p_WindowExtent)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto result =
        VKit::SwapChain::Builder(&device, p_Window.GetSurface())
            .RequestSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .RequestPresentMode(m_PresentMode)
            .RequestExtent(p_WindowExtent)
            .RequestImageCount(3)
            .SetOldSwapChain(m_SwapChain)
            .AddFlags(VKit::SwapChain::Builder::Flag_Clipped | VKit::SwapChain::Builder::Flag_CreateImageViews)
            .Build();

    VKIT_ASSERT_RESULT(result);
    m_SwapChain = result.GetValue();
#ifdef TKIT_ENABLE_DEBUG_LOGS
    const u32 icount = m_SwapChain.GetImageCount();
    TKIT_LOG_DEBUG("[ONYX] Created swapchain with {} images", icount);
#endif
}

void FrameScheduler::recreateSwapChain(Window &p_Window)
{
    TKIT_LOG_DEBUG("[ONYX] Out of date swap chain. Re-creating swap chain and resources");
    m_RequestSwapchainRecreation = false;
    const VkExtent2D extent = waitGlfwEvents(p_Window);
    Core::DeviceWaitIdle();

    VKit::SwapChain old = m_SwapChain;
    createSwapChain(p_Window, extent);
    old.Destroy();
    recreateResources();
    p_Window.adaptCamerasToViewportAspect();

    Event event{};
    event.Type = Event_SwapChainRecreated;
    p_Window.PushEvent(event);
}
void FrameScheduler::recreateSurface(Window &p_Window)
{
    TKIT_LOG_WARNING("[ONYX] Surface lost... re-creating surface, swap chain and resources");
    m_RequestSwapchainRecreation = false;
    const VkExtent2D extent = waitGlfwEvents(p_Window);
    Core::DeviceWaitIdle();

    m_SwapChain.Destroy();
    m_SwapChain = VKit::SwapChain{};

    p_Window.recreateSurface();
    createSwapChain(p_Window, extent);
    recreateResources();
}

void FrameScheduler::recreateResources()
{
    destroyImageData();
    m_Images = createImageData();

    Detail::DestroyPerImageSyncData(m_SyncImageData);
    Detail::DestroyPerFrameSyncData(m_SyncFrameData);
    m_SyncImageData = Detail::CreatePerImageSyncData(m_SwapChain.GetImageCount());
    m_SyncFrameData = Detail::CreatePerFrameSyncData();

    const PerImageData<VkImageView> imageViews = getIntermediateColorImageViews();
    m_PostProcessing->updateImageViews(imageViews);
    m_ImageIndex = 0;
}

PerImageData<FrameScheduler::ImageData> FrameScheduler::createImageData()
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    PerImageData<ImageData> images{};
    for (u32 i = 0; i < m_SwapChain.GetImageCount(); ++i)
    {
        ImageData data{};
        data.Presentation = &m_SwapChain.GetImage(i);

        auto iresult =
            VKit::Image::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), info.Extent, info.SurfaceFormat.format,
                                 VKit::Image::Flag_ColorAttachment | VKit::Image::Flag_Sampled)
                .WithImageView()
                .Build();
        VKIT_ASSERT_RESULT(iresult);
        data.Intermediate = iresult.GetValue();

        iresult = VKit::Image::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), info.Extent, s_DepthStencilFormat,
                                       VKit::Image::Flag_DepthAttachment | VKit::Image::Flag_StencilAttachment)
                      .WithImageView()
                      .Build();
        VKIT_ASSERT_RESULT(iresult);
        data.DepthStencil = iresult.GetValue();

        images.Append(data);
    }
    return images;
}
void FrameScheduler::destroyImageData()
{
    for (ImageData &data : m_Images)
    {
        data.Intermediate.Destroy();
        data.DepthStencil.Destroy();
    }
}

void FrameScheduler::createProcessingEffects()
{
    m_NaivePostProcessingFragmentShader = Pipelines::CreateShader(ONYX_ROOT_PATH "/onyx/shaders/pp-naive.frag");

    const PerImageData<VkImageView> imageviews = getIntermediateColorImageViews();
    m_PostProcessing.Construct(imageviews);

    const VKit::PipelineLayout::Builder builder = m_PostProcessing->CreatePipelineLayoutBuilder();
    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    m_NaivePostProcessingLayout = result.GetValue();

    setupNaivePostProcessing();
}

void FrameScheduler::createCommandData()
{
    const u32 gindex = Core::GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = Core::GetFamilyIndex(VKit::Queue_Transfer);

    if (gindex != tindex)
        m_TransferMode = Transfer_Separate;
    else if (m_QueueData.Graphics->Queue != m_QueueData.Transfer->Queue)
        m_TransferMode = Transfer_SameIndex;
    else
        m_TransferMode = Transfer_SameQueue;

    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
        auto result = VKit::CommandPool::Create(Core::GetDevice(), gindex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

        CommandData &cmd = m_CommandData[i];
        VKIT_ASSERT_RESULT(result);
        cmd.GraphicsPool = result.GetValue();

        auto cmdres = cmd.GraphicsPool.Allocate();
        VKIT_ASSERT_RESULT(cmdres);
        cmd.GraphicsCommand = cmdres.GetValue();
        if (m_TransferMode == Transfer_SameQueue)
        {
            cmd.TransferPool = cmd.GraphicsPool;
            cmd.TransferCommand = cmd.GraphicsCommand;
        }
        else if (m_TransferMode == Transfer_SameIndex)
        {
            cmd.TransferPool = cmd.GraphicsPool;
            cmdres = cmd.TransferPool.Allocate();
            VKIT_ASSERT_RESULT(cmdres);
            cmd.TransferCommand = cmdres.GetValue();
        }
        else
        {
            result = VKit::CommandPool::Create(Core::GetDevice(), tindex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            VKIT_ASSERT_RESULT(result);
            cmd.TransferPool = result.GetValue();

            cmdres = cmd.TransferPool.Allocate();
            VKIT_ASSERT_RESULT(cmdres);
            cmd.TransferCommand = cmdres.GetValue();
        }
    }
}

void FrameScheduler::setupNaivePostProcessing()
{
    PostProcessing::Specs specs{};
    specs.Layout = m_NaivePostProcessingLayout;
    specs.FragmentShader = m_NaivePostProcessingFragmentShader;
    specs.VertexShader = Pipelines::GetFullPassVertexShader();
    specs.SamplerCreateInfo = PostProcessing::DefaultSamplerCreateInfo();
    specs.RenderInfo = CreatePostProcessingRenderInfo();
    m_PostProcessing->Setup(specs);
}

PerImageData<VkImageView> FrameScheduler::getIntermediateColorImageViews() const
{
    PerImageData<VkImageView> imageViews;
    for (u32 i = 0; i < m_SwapChain.GetImageCount(); ++i)
        imageViews.Append(m_Images[i].Intermediate.GetImageView());
    return imageViews;
}

} // namespace Onyx
