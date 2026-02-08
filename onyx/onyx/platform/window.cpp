#include "onyx/core/pch.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/platform/glfw.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{

// edge cases a bit unrealistic yes
u32 ToFrequency(const TKit::Timespan deltaTime)
{
    const f32 seconds = deltaTime.AsSeconds();
    if (TKit::ApproachesZero(seconds))
        return TKIT_U32_MAX;
    if (seconds == TKIT_F32_MAX)
        return 0;

    return static_cast<u32>(1.f / deltaTime.AsSeconds()) + 1;
}
TKit::Timespan ToDeltaTime(const u32 frequency)
{
    if (frequency == 0)
        return TKit::Timespan::FromSeconds(TKIT_F32_MAX);
    if (frequency == TKIT_U32_MAX)
        return TKit::Timespan{};
    return TKit::Timespan::FromSeconds(1.f / static_cast<f32>(frequency));
}

VkExtent2D Window::waitGlfwEvents(const u32 width, const u32 height)
{
    VkExtent2D windowExtent = {width, height};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {width, height};
        glfwWaitEvents();
    }
    return windowExtent;
}

Result<VKit::SwapChain> Window::createSwapChain(const VkPresentModeKHR presentMode, const VkSurfaceKHR surface,
                                                const VkExtent2D &windowExtent, const VKit::SwapChain *old)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    return VKit::SwapChain::Builder(&device, surface)
        .RequestSurfaceFormat(Window::SurfaceFormat)
        .RequestPresentMode(presentMode)
        .RequestExtent(windowExtent)
        .RequestImageCount(3)
        .SetOldSwapChain(old ? *old : VK_NULL_HANDLE)
        .AddFlags(VKit::SwapChainBuilderFlag_Clipped | VKit::SwapChainBuilderFlag_CreateImageViews)
        .Build();
}

Result<TKit::TierArray<Window::ImageData>> Window::createImageData(VKit::SwapChain &swapChain)
{
    const VKit::SwapChain::Info &info = swapChain.GetInfo();
    TKit::TierArray<Window::ImageData> images{};
    for (u32 i = 0; i < swapChain.GetImageCount(); ++i)
    {
        Window::ImageData data{};
        data.Presentation = &swapChain.GetImage(i);

        const auto iresult = VKit::DeviceImage::Builder(
                                 Core::GetDevice(), Core::GetVulkanAllocator(), info.Extent, Window::DepthStencilFormat,
                                 VKit::DeviceImageFlag_DepthAttachment | VKit::DeviceImageFlag_StencilAttachment)
                                 .WithImageView()
                                 .Build();

        TKIT_RETURN_ON_ERROR(iresult);
        data.DepthStencil = iresult.GetValue();

        images.Append(data);
    }
    return images;
}

void Window::destroyImageData(TKit::TierArray<Window::ImageData> &images)
{
    for (Window::ImageData &data : images)
        data.DepthStencil.Destroy();
}

Window::~Window()
{
    Renderer::ClearWindow(this);
    VKIT_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    destroyImageData(m_Images);
    Execution::DestroySyncData(m_SyncData);

    m_SwapChain.Destroy();
    TKit::TierAllocator *alloc = TKit::GetTier();
    for (Camera<D2> *camera : m_Cameras2)
        alloc->Destroy(camera);
    for (Camera<D3> *camera : m_Cameras3)
        alloc->Destroy(camera);

    Core::GetInstanceTable()->DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

Result<bool> Window::handleImageResult(const VkResult result)
{
    if (result == VK_NOT_READY || result == VK_TIMEOUT)
        return false;

    if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        TKIT_RETURN_IF_FAILED(recreateSurface());
        return false;
    }

    const bool needRecreation =
        result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_MustRecreateSwapchain;

    if (!needRecreation && result != VK_SUCCESS)
        return Result<>::Error(result);

    if (needRecreation)
    {
        TKIT_RETURN_IF_FAILED(recreateSwapChain());
        return false;
    }
    return true;
}

Result<> Window::Present(const Renderer::RenderSubmitInfo &info)
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    const Execution::SyncData &sync = m_SyncData[m_ImageIndex];
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &sync.RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &m_ImageIndex;

    const auto table = Core::GetDeviceTable();
    const VkResult result = table->QueuePresentKHR(*m_Present, &presentInfo);

    TKIT_RETURN_IF_FAILED(handleImageResult(result));

    // a bit random that 0
    m_LastGraphicsSubmission.Timeline = info.SignalSemaphores[0].semaphore;
    m_LastGraphicsSubmission.InFlightValue = info.SignalSemaphores[0].value;

    return Result<>::Ok();
}

Result<bool> Window::AcquireNextImage(const Timeout timeout)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::AcquireNextImage");
    const auto table = Core::GetDeviceTable();
    const auto &device = Core::GetDevice();
    const auto acquire = [&]() {
        const u32 idx = (m_ImageAvailableIndex + 1) % m_SyncData.GetSize();
        const VkResult result = table->AcquireNextImageKHR(
            device, m_SwapChain, timeout, m_SyncData[idx].ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);

        if (result != VK_NOT_READY && result != VK_TIMEOUT)
            m_ImageAvailableIndex = idx;
        return handleImageResult(result);
    };

    if (timeout != Block)
        return acquire();

    if (m_LastGraphicsSubmission.Timeline)
    {
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_LastGraphicsSubmission.Timeline;
        waitInfo.pValues = &m_LastGraphicsSubmission.InFlightValue;

        const VkResult result = table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX);
        if (result != VK_SUCCESS)
            return Result<>::Error(result);
        m_LastGraphicsSubmission.Timeline = VK_NULL_HANDLE;
    }
    return acquire();
}

Result<> Window::createSwapChain(const VkExtent2D &windowExtent)
{
    const auto result = createSwapChain(m_PresentMode, m_Surface, windowExtent, &m_SwapChain);
    TKIT_RETURN_ON_ERROR(result);
    m_SwapChain = result.GetValue();
    return Result<>::Ok();
}

Result<> Window::recreateSwapChain()
{
    TKIT_LOG_DEBUG("[ONYX][WINDOW] Out of date swap chain. Re-creating swap chain and resources");
    const VkExtent2D extent = waitGlfwEvents(GetScreenWidth(), GetScreenHeight());

    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

    VKit::SwapChain old = m_SwapChain;
    TKIT_RETURN_IF_FAILED(createSwapChain(extent));
    old.Destroy();

    TKIT_RETURN_IF_FAILED(recreateResources());
    adaptCamerasToViewportAspect();

    Event event{};
    event.Type = Event_SwapChainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;
    return Result<>::Ok();
}

Result<> Window::recreateSurface()
{
    TKIT_LOG_WARNING("[ONYX][WINDOW] Surface lost... re-creating surface, swap chain and resources");
    const VkExtent2D extent = waitGlfwEvents(GetScreenWidth(), GetScreenHeight());

    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

    m_SwapChain.Destroy();
    m_SwapChain = VKit::SwapChain{};

    Core::GetInstanceTable()->DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    const VkResult sresult = glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface);
    if (sresult != VK_SUCCESS)
        return Result<>::Error(sresult);

    TKIT_RETURN_IF_FAILED(createSwapChain(extent));
    TKIT_RETURN_IF_FAILED(recreateResources());
    adaptCamerasToViewportAspect();

    Event event{};
    event.Type = Event_SwapChainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;
    return Result<>::Ok();
}

Result<> Window::recreateResources()
{
    const auto iresult = createImageData(m_SwapChain);
    TKIT_RETURN_ON_ERROR(iresult);
    destroyImageData(m_Images);
    m_Images = iresult.GetValue();

    const auto sresult = Execution::CreateSyncData(m_SwapChain.GetImageCount());
    TKIT_RETURN_ON_ERROR(sresult);
    Execution::DestroySyncData(m_SyncData);
    m_SyncData = sresult.GetValue();

    m_ImageIndex = 0;
    return Result<>::Ok();
}

void Window::BeginRendering(const VkCommandBuffer commandBuffer, const Color &clearColor)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginRendering");
    const auto table = Core::GetDeviceTable();

    VkRenderingAttachmentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    present.imageView = m_Images[m_ImageIndex].Presentation->GetImageView();
    present.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    present.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    present.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    present.clearValue.color = {{clearColor.rgba[0], clearColor.rgba[1], clearColor.rgba[2], clearColor.rgba[3]}};

    m_Images[m_ImageIndex].Presentation->TransitionLayout2(
        commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
         .SrcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
         .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = m_Images[m_ImageIndex].DepthStencil.GetImageView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    m_Images[m_ImageIndex].DepthStencil.TransitionLayout2(
        commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        {.DstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
         .SrcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
         .DstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR});

    const VkRenderingAttachmentInfoKHR stencil = depth;

    const VkExtent2D &extent = m_SwapChain.GetInfo().Extent;
    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &present;
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

    table->CmdSetViewport(commandBuffer, 0, 1, &viewport);
    table->CmdSetScissor(commandBuffer, 0, 1, &scissor);

    table->CmdBeginRenderingKHR(commandBuffer, &renderInfo);
}

void Window::EndRendering(const VkCommandBuffer commandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::EndRendering");
    const auto table = Core::GetDeviceTable();

    table->CmdEndRenderingKHR(commandBuffer);
    m_Images[m_ImageIndex].Presentation->TransitionLayout2(
        commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        {.SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
         .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
         .DstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR});
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

TKit::Timespan Window::UpdateMonitorDeltaTime(const TKit::Timespan tdefault)
{
    GLFWmonitor *monitor = glfwGetWindowMonitor(m_Window);
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        m_MonitorDeltaTime = tdefault;
        return tdefault;
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    m_MonitorDeltaTime = TKit::Timespan::FromSeconds(1.f / static_cast<f32>(mode->refreshRate));
    return m_MonitorDeltaTime;
}

void Window::FlagShouldClose()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PushEvent(const Event &event)
{
    if (!m_Events.IsFull())
        m_Events.Append(event);
}

void Window::FlushEvents()
{
    m_Events.Clear();
}

void Window::adaptCamerasToViewportAspect()
{
    for (Camera<D2> *cam : m_Cameras2)
        cam->adaptViewToViewportAspect();
    for (Camera<D3> *cam : m_Cameras3)
        cam->adaptViewToViewportAspect();
}

} // namespace Onyx
