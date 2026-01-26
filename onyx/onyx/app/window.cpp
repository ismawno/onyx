#include "onyx/core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/macros.hpp"
#include "onyx/core/glfw.hpp"

namespace Onyx
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
static u32 s_ColorIndex = 0;
static u32 s_Colors[4] = {0x434E78, 0x607B8F, 0xF7E396, 0xE97F4A};
#endif

static u64 s_ViewCache = TKIT_U64_MAX;
static u64 allocateViewBit()
{
    const u32 index = static_cast<u32>(std::countr_zero(s_ViewCache));
    const u64 viewBit = 1 << index;
    s_ViewCache &= ~viewBit;
    return viewBit;
}
static void deallocateViewBit(const u64 viewBit)
{
    s_ViewCache |= viewBit;
}

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

static VkExtent2D waitGlfwEvents(const u32 width, const u32 height)
{
    VkExtent2D windowExtent = {width, height};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {width, height};
        glfwWaitEvents();
    }
    return windowExtent;
}

ONYX_NO_DISCARD static Result<VKit::SwapChain> createSwapChain(const VkPresentModeKHR presentMode,
                                                               const VkSurfaceKHR surface,
                                                               const VkExtent2D &windowExtent,
                                                               const VKit::SwapChain *old = nullptr)
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

ONYX_NO_DISCARD static Result<TKit::TierArray<Window::ImageData>> createImageData(VKit::SwapChain &swapChain)
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

static void destroyImageData(TKit::TierArray<Window::ImageData> &images)
{
    for (Window::ImageData &data : images)
        data.DepthStencil.Destroy();
}

Result<Window *> Window::Create()
{
    return Create({});
}
Result<Window *> Window::Create(const Specs &specs)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, specs.Flags & WindowFlag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, specs.Flags & WindowFlag_Visible);
    glfwWindowHint(GLFW_DECORATED, specs.Flags & WindowFlag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, specs.Flags & WindowFlag_Focused);
    glfwWindowHint(GLFW_FLOATING, specs.Flags & WindowFlag_Floating);

    GLFWwindow *handle = glfwCreateWindow(static_cast<i32>(specs.Dimensions[0]), static_cast<i32>(specs.Dimensions[1]),
                                          specs.Name, nullptr, nullptr);
    if (!handle)
        return Result<>::Error(Error_RejectedWindow);

    VKit::DeletionQueue dqueue{};
    dqueue.Push([handle] { glfwDestroyWindow(handle); });

    u32v2 pos;
    if (specs.Position != u32v2{TKIT_U32_MAX})
    {
        glfwSetWindowPos(handle, static_cast<i32>(specs.Position[0]), static_cast<i32>(specs.Position[1]));
        pos = specs.Position;
    }
    else
    {
        i32 x, y;
        glfwGetWindowPos(handle, &x, &y);
        pos = u32v2{x, y};
    }

    VkSurfaceKHR surface;
    const VkResult result = glfwCreateWindowSurface(Core::GetInstance(), handle, nullptr, &surface);
    if (result != VK_SUCCESS)
        return Result<>::Error(Error_NoSurfaceCapabilities);

    dqueue.Push([surface] { Core::GetInstanceTable()->DestroySurfaceKHR(Core::GetInstance(), surface, nullptr); });
    if (specs.Flags & WindowFlag_InstallCallbacks)
        Input::InstallCallbacks(handle);

    const VkExtent2D extent = waitGlfwEvents(specs.Dimensions[0], specs.Dimensions[1]);
    auto sresult = Onyx::createSwapChain(specs.PresentMode, surface, extent);
    TKIT_RETURN_ON_ERROR(sresult);

    VKit::SwapChain &schain = sresult.GetValue();
    dqueue.Push([&schain] { schain.Destroy(); });

    auto syresult = Execution::CreateSyncData(schain.GetImageCount());
    TKIT_RETURN_ON_ERROR(syresult);
    TKit::TierArray<Execution::SyncData> &syncData = syresult.GetValue();
    dqueue.Push([&syncData] { Execution::DestroySyncData(syncData); });

    if (s_ViewCache == 0)
        return Result<>::Error(
            Error_InitializationFailed,
            "[ONYX][WINDOW] Maximum amount of windows exceeded. There is a hard limit of 64 windows");

    TKit::TierAllocator *alloc = TKit::Memory::GetTier();
    Window *window = alloc->Create<Window>();

    window->m_Window = handle;
    window->m_Surface = surface;
    window->m_SwapChain = schain;
    window->m_Position = pos;
    window->m_Dimensions = specs.Dimensions;

    auto iresult = createImageData(window->m_SwapChain);
    TKIT_RETURN_ON_ERROR(iresult);
    TKit::TierArray<ImageData> &imageData = iresult.GetValue();
    dqueue.Push([&imageData] { destroyImageData(imageData); });
    window->m_Images = std::move(imageData);

    window->m_SyncData = std::move(syncData);
    window->m_ViewBit = allocateViewBit();
    window->m_Present = Execution::FindSuitableQueue(VKit::Queue_Present);
#ifdef TKIT_ENABLE_INSTRUMENTATION
    window->m_ColorIndex = s_ColorIndex++ & 3;
#endif
    window->UpdateMonitorDeltaTime();
    glfwSetWindowUserPointer(handle, window);

    dqueue.Clear();
    return window;
}

void Window::Destroy(Window *window)
{
    TKit::TierAllocator *alloc = TKit::Memory::GetTier();
    alloc->Destroy(window);
}

Window::~Window()
{
    deallocateViewBit(m_ViewBit);
    Renderer::ClearWindow(this);
    ONYX_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    destroyImageData(m_Images);
    Execution::DestroySyncData(m_SyncData);

    m_SwapChain.Destroy();
    TKit::TierAllocator *alloc = TKit::Memory::GetTier();
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
        m_ImageAvailableIndex = (m_ImageAvailableIndex + 1) % m_SyncData.GetSize();
        const VkResult result = table->AcquireNextImageKHR(device, m_SwapChain, timeout,
                                                           m_SyncData[m_ImageAvailableIndex].ImageAvailableSemaphore,
                                                           VK_NULL_HANDLE, &m_ImageIndex);
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
    const auto result = Onyx::createSwapChain(m_PresentMode, m_Surface, windowExtent, &m_SwapChain);
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
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    const auto table = Core::GetDeviceTable();

    VkRenderingAttachmentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    present.imageView = m_Images[m_ImageIndex].Presentation->GetImageView();
    present.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    present.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    present.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    present.clearValue.color = {{clearColor.RGBA[0], clearColor.RGBA[1], clearColor.RGBA[2], clearColor.RGBA[3]}};

    m_Images[m_ImageIndex].Presentation->TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

    m_Images[m_ImageIndex].DepthStencil.TransitionLayout(commandBuffer,
                                                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    const auto table = Core::GetDeviceTable();

    table->CmdEndRenderingKHR(commandBuffer);
    m_Images[m_ImageIndex].Presentation->TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                          {.SrcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                           .DstAccess = 0,
                                                           .SrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                           .DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT});
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
