#include "onyx/core/pch.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/input.hpp"
#include "onyx/core/core.hpp"
#include "onyx/platform/glfw.hpp"
#include "onyx/property/camera.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

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

    return u32(1.f / deltaTime.AsSeconds()) + 1;
}
TKit::Timespan ToDeltaTime(const u32 frequency)
{
    if (frequency == 0)
        return TKit::Timespan::FromSeconds(TKIT_F32_MAX);
    if (frequency == TKIT_U32_MAX)
        return TKit::Timespan{};
    return TKit::Timespan::FromSeconds(1.f / f32(frequency));
}

Window *Window::FromHandle(GLFWwindow *window)
{
    return scast<Window *>(glfwGetWindowUserPointer(window));
}

void Window::Show()
{
    glfwShowWindow(m_Window);
}
void Window::Focus()
{
    glfwFocusWindow(m_Window);
}

bool Window::CanQueryOpacity() const
{
#ifdef ONYX_GLFW_WINDOW_OPACITY
    return true;
#else
    return false;
#endif
}

const char *Window::GetTitle() const
{
    return glfwGetWindowTitle(m_Window);
}

i32v2 Window::GetPosition() const
{
    i32 x;
    i32 y;
    glfwGetWindowPos(m_Window, &x, &y);
    return i32v2{x, y};
}

u32v2 Window::GetScreenDimensions() const
{
    i32 w;
    i32 h;
    glfwGetWindowSize(m_Window, &w, &h);
    return u32v2{w, h};
}

u32v2 Window::GetPixelDimensions() const
{
    i32 w;
    i32 h;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return u32v2{w, h};
}

f32 Window::GetAspect() const
{
    const u32v2 pdim = GetPixelDimensions();
    return f32(pdim[1]) / f32(pdim[0]);
}

f32 Window::GetOpacity() const
{
#ifdef ONYX_GLFW_WINDOW_OPACITY
    return glfwGetWindowOpacity(m_Window);
#else
    TKIT_FATAL("[ONYX][WINDOW] To query opacity, GLFW 3.3 or greater is required. Use CanQueryOpacity() to check if "
               "the feature is available");
    return 0.f;
#endif
}
void Window::SetOpacity(const f32 opacity)
{
#ifdef ONYX_GLFW_WINDOW_OPACITY
    glfwSetWindowOpacity(m_Window, opacity);
#else
    TKIT_FATAL("[ONYX][WINDOW] To query opacity, GLFW 3.3 or greater is required. Use CanQueryOpacity() to check if "
               "the feature is available");
#endif
}

WindowFlags Window::GetFlags() const
{
    WindowFlags flags = 0;
    if (glfwGetWindowAttrib(m_Window, GLFW_RESIZABLE))
        flags |= WindowFlag_Resizable;
    if (glfwGetWindowAttrib(m_Window, GLFW_VISIBLE))
        flags |= WindowFlag_Visible;
    if (glfwGetWindowAttrib(m_Window, GLFW_DECORATED))
        flags |= WindowFlag_Decorated;
    if (glfwGetWindowAttrib(m_Window, GLFW_FOCUSED))
        flags |= WindowFlag_Focused;
    if (glfwGetWindowAttrib(m_Window, GLFW_FLOATING))
        flags |= WindowFlag_Floating;
    if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
        flags |= WindowFlag_Iconified;
#ifdef ONYX_GLFW_FOCUS_ON_SHOW
    if (glfwGetWindowAttrib(m_Window, GLFW_FOCUS_ON_SHOW))
        flags |= WindowFlag_FocusOnShow;
#endif
    return flags;
}
void Window::SetFlags(const WindowFlags flags)
{
    glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, flags & WindowFlag_Resizable);
    glfwSetWindowAttrib(m_Window, GLFW_VISIBLE, flags & WindowFlag_Visible);
    glfwSetWindowAttrib(m_Window, GLFW_DECORATED, flags & WindowFlag_Decorated);
    glfwSetWindowAttrib(m_Window, GLFW_FOCUSED, flags & WindowFlag_Focused);
    glfwSetWindowAttrib(m_Window, GLFW_FLOATING, flags & WindowFlag_Floating);
    glfwSetWindowAttrib(m_Window, GLFW_ICONIFIED, flags & WindowFlag_Iconified);
}
void Window::AddFlags(const WindowFlags flags)
{
    if (flags & WindowFlag_Resizable)
        glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, GLFW_TRUE);
    if (flags & WindowFlag_Visible)
        glfwSetWindowAttrib(m_Window, GLFW_VISIBLE, GLFW_TRUE);
    if (flags & WindowFlag_Decorated)
        glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_TRUE);
    if (flags & WindowFlag_Focused)
        glfwSetWindowAttrib(m_Window, GLFW_FOCUSED, GLFW_TRUE);
    if (flags & WindowFlag_Floating)
        glfwSetWindowAttrib(m_Window, GLFW_FLOATING, GLFW_TRUE);
    if (flags & WindowFlag_Iconified)
        glfwSetWindowAttrib(m_Window, GLFW_ICONIFIED, GLFW_TRUE);
}
void Window::RemoveFlags(const WindowFlags flags)
{
    if (flags & WindowFlag_Resizable)
        glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, GLFW_FALSE);
    if (flags & WindowFlag_Visible)
        glfwSetWindowAttrib(m_Window, GLFW_VISIBLE, GLFW_FALSE);
    if (flags & WindowFlag_Decorated)
        glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_FALSE);
    if (flags & WindowFlag_Focused)
        glfwSetWindowAttrib(m_Window, GLFW_FOCUSED, GLFW_FALSE);
    if (flags & WindowFlag_Floating)
        glfwSetWindowAttrib(m_Window, GLFW_FLOATING, GLFW_FALSE);
    if (flags & WindowFlag_Iconified)
        glfwSetWindowAttrib(m_Window, GLFW_ICONIFIED, GLFW_FALSE);
}

void Window::SetTitle(const char *title)
{
    glfwSetWindowTitle(m_Window, title);
}
void Window::SetPosition(const i32v2 &pos)
{
    glfwSetWindowPos(m_Window, pos[0], pos[1]);
}
void Window::SetScreenDimensions(const u32v2 &dim)
{
#if defined(TKIT_OS_APPLE) && !defined(ONYX_GLFW_OSX_WINDOW_POS_FIX)
    i32 x;
    i32 y;
    i32 w;
    i32 h;
    glfwGetWindowPos(m_Window, &x, &y);
    glfwGetWindowSize(m_Window, &w, &h);
    glfwSetWindowPos(m_Window, x, y - h + i32(dim[1]));
#endif
    glfwSetWindowSize(m_Window, i32(dim[0]), i32(dim[1]));
    // m_MustRecreateSwapchain = true;
}
void Window::SetAspect(const u32 numer, const u32 denom)
{
    glfwSetWindowAspectRatio(m_Window, i32(numer), i32(denom));
}

VkExtent2D Window::getNewExtent(GLFWwindow *window)
{
    i32 w = 0;
    i32 h = 0;

    while (w == 0 || h == 0)
        glfwGetFramebufferSize(window, &w, &h);

    return VkExtent2D{u32(w), u32(h)};
}

Result<VKit::SwapChain> Window::createSwapChain(const VkPresentModeKHR presentMode, const VkSurfaceKHR surface,
                                                const VkExtent2D &windowExtent, const VKit::SwapChain *old)
{
    const VKit::LogicalDevice &device = GetDevice();
    return VKit::SwapChain::Builder(&device, surface)
        .RequestSurfaceFormat(Platform::GetSurfaceFormat())
        .RequestPresentMode(presentMode)
        .RequestExtent(windowExtent)
        .RequestImageCount(3)
        .SetOldSwapChain(old ? *old : VK_NULL_HANDLE)
        .AddFlags(VKit::SwapChainBuilderFlag_Clipped | VKit::SwapChainBuilderFlag_CreateImageViews)
        .Build();
}

// TODO(Isma): Should handle errors better. there are leaks here when something fails
Result<TKit::TierArray<WindowSyncData>> Window::createSyncData(const u32 imageCount)
{
    const auto &device = GetDevice();
    const auto table = GetDeviceTable();

    TKit::TierArray<WindowSyncData> syncs{};
    syncs.Resize(imageCount);
    for (u32 i = 0; i < imageCount; ++i)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        syncs[i].Tracker = {};
        VKIT_RETURN_IF_FAILED(
            table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].ImageAvailableSemaphore), Result<>,
            destroySyncData(syncs));
        VKIT_RETURN_IF_FAILED(
            table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].RenderFinishedSemaphore), Result<>,
            destroySyncData(syncs));
    }
    return syncs;
}

void Window::destroySyncData(const TKit::TierArray<WindowSyncData> &sync)
{
    const auto &device = GetDevice();
    const auto table = GetDeviceTable();
    for (const WindowSyncData &data : sync)
    {
        table->DestroySemaphore(device, data.ImageAvailableSemaphore, nullptr);
        table->DestroySemaphore(device, data.RenderFinishedSemaphore, nullptr);
    }
}

TKit::TierArray<VKit::DeviceImage *> Window::extractSwapChainImages(VKit::SwapChain &swapChain)
{
    TKit::TierArray<VKit::DeviceImage *> images{};
    for (u32 i = 0; i < swapChain.GetImageCount(); ++i)
        images.Append(&swapChain.GetImage(i));
    return images;
}

Window::~Window()
{
    // ONYX_CHECK_EXPRESSION(DeviceWaitIdle());
    ONYX_CHECK_EXPRESSION(drainWork());
    destroySyncData(m_SyncData);

    m_SwapChain.Destroy();

    TKit::TierAllocator *tier = TKit::GetTier();
    for (RenderView<D2> *rv : m_RenderViews2)
        tier->Destroy(rv);
    for (RenderView<D3> *rv : m_RenderViews3)
        tier->Destroy(rv);

    GetInstanceTable()->DestroySurfaceKHR(GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

Result<bool> Window::handlePresentOrAcquireResult(const VkResult result)
{
    TKIT_LOG_DEBUG_IF(result != VK_SUCCESS, "[ONYX][WINDOW] Present() or AcquireNextImage() returned '{}'",
                      VKit::VulkanResultToString(result));

    if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        TKIT_RETURN_IF_FAILED(recreateSurface());
        return false;
    }

    if (m_MustRecreateSwapchain || result == VK_ERROR_OUT_OF_DATE_KHR ||
        (result == VK_SUBOPTIMAL_KHR && m_TimeSinceResize.GetElapsed().As<TKit::Timespan::Milliseconds, u64>() > 350))
    {
        TKIT_RETURN_IF_FAILED(recreateSwapChain());
        return false;
    }
    else if (result == VK_SUBOPTIMAL_KHR)
        return true;

    VKIT_RETURN_IF_FAILED(result, Result<bool>);
    return true;
}

Result<> Window::nameSurface()
{
    const auto &device = GetDevice();
    const std::string name = TKit::Format("onyx-surface-window-'{}'", GetTitle());
    return device.SetObjectName(m_Surface, VK_OBJECT_TYPE_SURFACE_KHR, name.c_str());
}

Result<> Window::nameSwapChain()
{
    const std::string name = TKit::Format("onyx-swapchain-window-'{}'", GetTitle());
    return m_SwapChain.SetName(name.c_str());
}

Result<> Window::nameSyncData()
{
    const auto &device = GetDevice();
    const char *title = GetTitle();
    for (u32 i = 0; i < m_SwapChain.GetImageCount(); ++i)
    {
        const std::string rfinish = TKit::Format("onyx-render-finished-semaphore-window-'{}'-image-index-{}", title, i);
        const std::string iavail = TKit::Format("onyx-image-available-semaphore-index-{}-window-'{}'", i, title);
        TKIT_RETURN_IF_FAILED(
            device.SetObjectName(m_SyncData[i].RenderFinishedSemaphore, VK_OBJECT_TYPE_SEMAPHORE, rfinish.c_str()));
        TKIT_RETURN_IF_FAILED(
            device.SetObjectName(m_SyncData[i].ImageAvailableSemaphore, VK_OBJECT_TYPE_SEMAPHORE, iavail.c_str()));
    }
    return Result<>::Ok();
}

Result<> Window::nameSwapChainImages()
{
    const char *title = GetTitle();
    for (u32 i = 0; i < m_SwapChain.GetImageCount(); ++i)
    {
        const std::string pres = TKit::Format("onyx-presentation-image-index-{}-window-'{}'", i, title);
        TKIT_RETURN_IF_FAILED(m_Presentation[i]->SetName(pres.c_str()));
    }
    return Result<>::Ok();
}

Result<> Window::Present()
{
    TKIT_PROFILE_NSCOPE("Onyx::FramwScheduler::Present");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    const WindowSyncData &sync = m_SyncData[m_ImageIndex];
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &sync.RenderFinishedSemaphore;

    const VkSwapchainKHR swapChain = m_SwapChain;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &m_ImageIndex;

    const auto table = GetDeviceTable();
    const VkResult result = table->QueuePresentKHR(*m_Present, &presentInfo);

    TKIT_RETURN_IF_FAILED(handlePresentOrAcquireResult(result));

    return Result<>::Ok();
}

Result<bool> Window::AcquireNextImage(const Timeout timeout)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::AcquireNextImage");
    const auto table = GetDeviceTable();
    const auto &device = GetDevice();

    const u32 idx = (m_SyncIndex + 1) % m_SyncData.GetSize();
    const WindowSyncData &sync = m_SyncData[idx];

    if (sync.Tracker.InFlight())
    {
        const VkSemaphore sm = sync.Tracker.Queue->GetTimelineSempahore();
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &sm;
        waitInfo.pValues = &sync.Tracker.InFlightValue;

        const VkResult result = table->WaitSemaphoresKHR(device, &waitInfo, timeout);
        if (result == VK_NOT_READY || result == VK_TIMEOUT)
            return false;

        VKIT_RETURN_ON_ERROR(result, Result<>);
    }
    const VkResult result = table->AcquireNextImageKHR(device, m_SwapChain, timeout, sync.ImageAvailableSemaphore,
                                                       VK_NULL_HANDLE, &m_ImageIndex);

    if (result != VK_NOT_READY && result != VK_TIMEOUT)
    {
        m_SyncIndex = idx;
        for (RenderView<D2> *rv : m_RenderViews2)
            rv->acquireImage(m_ImageIndex);
        for (RenderView<D3> *rv : m_RenderViews3)
            rv->acquireImage(m_ImageIndex);
        return handlePresentOrAcquireResult(result);
    }
    return false;
}

Result<> Window::createSwapChain(const VkExtent2D &windowExtent)
{
    const auto result = createSwapChain(m_PresentMode, m_Surface, windowExtent, &m_SwapChain);
    TKIT_RETURN_ON_ERROR(result);
    m_SwapChain = result.GetValue();
    return Result<>::Ok();
}

Result<> Window::drainWork()
{
    TKit::StackArray<VkSemaphore> semaphores{};
    semaphores.Reserve(m_SyncData.GetSize());
    TKit::StackArray<u64> values{};
    values.Reserve(m_SyncData.GetSize());
    for (const WindowSyncData &sync : m_SyncData)
        if (sync.Tracker.InFlight())
        {
            semaphores.Append(sync.Tracker.Queue->GetTimelineSempahore());
            values.Append(sync.Tracker.InFlightValue);
        }

    if (!semaphores.IsEmpty())
    {
        const auto table = GetDeviceTable();
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = semaphores.GetSize();
        waitInfo.pSemaphores = semaphores.GetData();
        waitInfo.pValues = values.GetData();

        const auto &device = GetDevice();
        VKIT_RETURN_IF_FAILED(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX), Result<>);
    }

    return m_Present->WaitIdle();
}

Result<> Window::recreateSwapChain()
{
    TKIT_LOG_DEBUG("[ONYX][WINDOW] Out of date swap chain. Re-creating swap chain and resources");

    TKIT_RETURN_IF_FAILED(drainWork());
    const VkExtent2D extent = getNewExtent(m_Window);

    VKit::SwapChain old = m_SwapChain;
    TKIT_RETURN_IF_FAILED(createSwapChain(extent));
    old.Destroy();

    m_Presentation = extractSwapChainImages(m_SwapChain);
    TKIT_RETURN_IF_FAILED(recreateResources());
    TKIT_RETURN_IF_FAILED(updateRenderViews());

    Event event{};
    event.Type = Event_SwapChainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;

    if (IsDebugUtilsEnabled())
    {
        TKIT_RETURN_IF_FAILED(nameSwapChain());
        TKIT_RETURN_IF_FAILED(nameSyncData());
        return nameSwapChainImages();
    }
    return Result<>::Ok();
}

Result<> Window::recreateSurface()
{
    TKIT_LOG_WARNING("[ONYX][WINDOW] Surface lost... re-creating surface, swap chain and resources");
    TKIT_RETURN_IF_FAILED(drainWork());

    const VkExtent2D extent = getNewExtent(m_Window);

    m_SwapChain.Destroy();
    m_SwapChain = VKit::SwapChain{};

    GetInstanceTable()->DestroySurfaceKHR(GetInstance(), m_Surface, nullptr);
    VKIT_RETURN_IF_FAILED(glfwCreateWindowSurface(GetInstance(), m_Window, nullptr, &m_Surface), Result<>);

    TKIT_RETURN_IF_FAILED(createSwapChain(extent));
    TKIT_RETURN_IF_FAILED(recreateResources());
    TKIT_RETURN_IF_FAILED(updateRenderViews());

    Event event{};
    event.Type = Event_SwapChainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;
    if (IsDebugUtilsEnabled())
    {
        TKIT_RETURN_IF_FAILED(nameSurface());
        TKIT_RETURN_IF_FAILED(nameSwapChain());
        TKIT_RETURN_IF_FAILED(nameSyncData());
        return nameSwapChainImages();
    }
    return Result<>::Ok();
}

Result<> Window::recreateResources()
{
    const auto sresult = createSyncData(m_SwapChain.GetImageCount());
    TKIT_RETURN_ON_ERROR(sresult);
    destroySyncData(m_SyncData);
    m_SyncData = sresult.GetValue();

    m_ImageIndex = 0;
    return Result<>::Ok();
}

void Window::BeginRendering(const VkCommandBuffer cmd, const Execution::Tracker &tracker)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginRendering");

    m_SyncData[m_SyncIndex].Tracker = tracker;
    VKit::DeviceImage *pimage = m_Presentation[m_ImageIndex];
    VkRenderingAttachmentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    present.imageView = pimage->GetView();
    present.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    present.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    present.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    present.clearValue.color = {{ClearColor.rgba[0], ClearColor.rgba[1], ClearColor.rgba[2], ClearColor.rgba[3]}};

    pimage->TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                               .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                               .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    const VkExtent2D &extent = m_SwapChain.GetInfo().Extent;
    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &present;

    // VkViewport viewport{};
    // viewport.x = 0.f;
    // viewport.y = 0.f;
    // viewport.width = f32(extent.width);
    // viewport.height = f32(extent.height);
    // viewport.minDepth = 0.f;
    // viewport.maxDepth = 1.f;
    //
    // VkRect2D scissor{};
    // scissor.offset = {0, 0};
    // scissor.extent = extent;
    //
    const auto table = GetDeviceTable();
    // table->CmdSetViewport(cmd, 0, 1, &viewport);
    // table->CmdSetScissor(cmd, 0, 1, &scissor);

    table->CmdBeginRenderingKHR(cmd, &renderInfo);
}

void Window::EndRendering(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::EndRendering");
    const auto table = GetDeviceTable();

    table->CmdEndRenderingKHR(cmd);
    m_Presentation[m_ImageIndex]->TransitionLayout2(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                    {.SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                                     .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                     .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});
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

    m_MonitorDeltaTime = TKit::Timespan::FromSeconds(1.f / f32(mode->refreshRate));
    return m_MonitorDeltaTime;
}

void Window::FlagShouldClose()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PushEvent(const Event &event)
{
    m_Events.Append(event);
}

void Window::FlushEvents()
{
    m_Events.Clear();
}

template <Dimension D>
Result<RenderView<D> *> Window::CreateRenderView(Camera<D> *camera, RenderViewFlags flags,
                                                 const ScreenViewport &viewport, const ScreenScissor &scissor)
{
    TKit::TierHive<RenderView<D> *> &rdata = getRenderViews<D>();
    const u32 offset = rdata.Insert(nullptr);

    TKit::TierAllocator *tier = TKit::GetTier();
    RenderView<D> *rv = tier->Create<RenderView<D>>(m_SwapChain.GetInfo().Extent, m_CompositorSet, offset, camera,
                                                    flags, viewport, scissor);

    rdata[offset] = rv;

    const auto result = rv->createFramebuffers(m_SwapChain.GetImageCount());
    TKIT_RETURN_ON_ERROR(result, tier->Destroy(rv); rdata.Remove(offset));
    rv->m_FrameBuffers = result.GetValue();
    rv->acquireImage(m_ImageIndex);

    return rv;
}

template <Dimension D> void Window::DestroyRenderView(RenderView<D> *rv)
{
    TKit::TierHive<RenderView<D> *> &rvs = getRenderViews<D>();
    rvs.Remove(rv->GetId());

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(rv);
}

Result<> Window::updateRenderViews()
{
    const VkExtent2D &extent = m_SwapChain.GetInfo().Extent;
    for (RenderView<D2> *rv : m_RenderViews2)
        TKIT_RETURN_IF_FAILED(rv->update(extent, m_SwapChain.GetImageCount()));
    for (RenderView<D3> *rv : m_RenderViews3)
        TKIT_RETURN_IF_FAILED(rv->update(extent, m_SwapChain.GetImageCount()));
    return Result<>::Ok();
}

static i32 toGlfw(const Key key)
{
    switch (key)
    {
    case Key_Space:
        return GLFW_KEY_SPACE;
    case Key_Apostrophe:
        return GLFW_KEY_APOSTROPHE;
    case Key_Comma:
        return GLFW_KEY_COMMA;
    case Key_Minus:
        return GLFW_KEY_MINUS;
    case Key_Period:
        return GLFW_KEY_PERIOD;
    case Key_Slash:
        return GLFW_KEY_SLASH;
    case Key_N0:
        return GLFW_KEY_0;
    case Key_N1:
        return GLFW_KEY_1;
    case Key_N2:
        return GLFW_KEY_2;
    case Key_N3:
        return GLFW_KEY_3;
    case Key_N4:
        return GLFW_KEY_4;
    case Key_N5:
        return GLFW_KEY_5;
    case Key_N6:
        return GLFW_KEY_6;
    case Key_N7:
        return GLFW_KEY_7;
    case Key_N8:
        return GLFW_KEY_8;
    case Key_N9:
        return GLFW_KEY_9;
    case Key_Semicolon:
        return GLFW_KEY_SEMICOLON;
    case Key_Equal:
        return GLFW_KEY_EQUAL;
    case Key_A:
        return GLFW_KEY_A;
    case Key_B:
        return GLFW_KEY_B;
    case Key_C:
        return GLFW_KEY_C;
    case Key_D:
        return GLFW_KEY_D;
    case Key_E:
        return GLFW_KEY_E;
    case Key_F:
        return GLFW_KEY_F;
    case Key_G:
        return GLFW_KEY_G;
    case Key_H:
        return GLFW_KEY_H;
    case Key_I:
        return GLFW_KEY_I;
    case Key_J:
        return GLFW_KEY_J;
    case Key_K:
        return GLFW_KEY_K;
    case Key_L:
        return GLFW_KEY_L;
    case Key_M:
        return GLFW_KEY_M;
    case Key_N:
        return GLFW_KEY_N;
    case Key_O:
        return GLFW_KEY_O;
    case Key_P:
        return GLFW_KEY_P;
    case Key_Q:
        return GLFW_KEY_Q;
    case Key_R:
        return GLFW_KEY_R;
    case Key_S:
        return GLFW_KEY_S;
    case Key_T:
        return GLFW_KEY_T;
    case Key_U:
        return GLFW_KEY_U;
    case Key_V:
        return GLFW_KEY_V;
    case Key_W:
        return GLFW_KEY_W;
    case Key_X:
        return GLFW_KEY_X;
    case Key_Y:
        return GLFW_KEY_Y;
    case Key_Z:
        return GLFW_KEY_Z;
    case Key_LeftBracket:
        return GLFW_KEY_LEFT_BRACKET;
    case Key_Backslash:
        return GLFW_KEY_BACKSLASH;
    case Key_RightBracket:
        return GLFW_KEY_RIGHT_BRACKET;
    case Key_GraveAccent:
        return GLFW_KEY_GRAVE_ACCENT;
    case Key_World_1:
        return GLFW_KEY_WORLD_1;
    case Key_World_2:
        return GLFW_KEY_WORLD_2;
    case Key_Escape:
        return GLFW_KEY_ESCAPE;
    case Key_Enter:
        return GLFW_KEY_ENTER;
    case Key_Tab:
        return GLFW_KEY_TAB;
    case Key_Backspace:
        return GLFW_KEY_BACKSPACE;
    case Key_Insert:
        return GLFW_KEY_INSERT;
    case Key_Delete:
        return GLFW_KEY_DELETE;
    case Key_Right:
        return GLFW_KEY_RIGHT;
    case Key_Left:
        return GLFW_KEY_LEFT;
    case Key_Down:
        return GLFW_KEY_DOWN;
    case Key_Up:
        return GLFW_KEY_UP;
    case Key_PageUp:
        return GLFW_KEY_PAGE_UP;
    case Key_PageDown:
        return GLFW_KEY_PAGE_DOWN;
    case Key_Home:
        return GLFW_KEY_HOME;
    case Key_End:
        return GLFW_KEY_END;
    case Key_CapsLock:
        return GLFW_KEY_CAPS_LOCK;
    case Key_ScrollLock:
        return GLFW_KEY_SCROLL_LOCK;
    case Key_NumLock:
        return GLFW_KEY_NUM_LOCK;
    case Key_PrintScreen:
        return GLFW_KEY_PRINT_SCREEN;
    case Key_Pause:
        return GLFW_KEY_PAUSE;
    case Key_F1:
        return GLFW_KEY_F1;
    case Key_F2:
        return GLFW_KEY_F2;
    case Key_F3:
        return GLFW_KEY_F3;
    case Key_F4:
        return GLFW_KEY_F4;
    case Key_F5:
        return GLFW_KEY_F5;
    case Key_F6:
        return GLFW_KEY_F6;
    case Key_F7:
        return GLFW_KEY_F7;
    case Key_F8:
        return GLFW_KEY_F8;
    case Key_F9:
        return GLFW_KEY_F9;
    case Key_F10:
        return GLFW_KEY_F10;
    case Key_F11:
        return GLFW_KEY_F11;
    case Key_F12:
        return GLFW_KEY_F12;
    case Key_F13:
        return GLFW_KEY_F13;
    case Key_F14:
        return GLFW_KEY_F14;
    case Key_F15:
        return GLFW_KEY_F15;
    case Key_F16:
        return GLFW_KEY_F16;
    case Key_F17:
        return GLFW_KEY_F17;
    case Key_F18:
        return GLFW_KEY_F18;
    case Key_F19:
        return GLFW_KEY_F19;
    case Key_F20:
        return GLFW_KEY_F20;
    case Key_F21:
        return GLFW_KEY_F21;
    case Key_F22:
        return GLFW_KEY_F22;
    case Key_F23:
        return GLFW_KEY_F23;
    case Key_F24:
        return GLFW_KEY_F24;
    case Key_F25:
        return GLFW_KEY_F25;
    case Key_KP_0:
        return GLFW_KEY_KP_0;
    case Key_KP_1:
        return GLFW_KEY_KP_1;
    case Key_KP_2:
        return GLFW_KEY_KP_2;
    case Key_KP_3:
        return GLFW_KEY_KP_3;
    case Key_KP_4:
        return GLFW_KEY_KP_4;
    case Key_KP_5:
        return GLFW_KEY_KP_5;
    case Key_KP_6:
        return GLFW_KEY_KP_6;
    case Key_KP_7:
        return GLFW_KEY_KP_7;
    case Key_KP_8:
        return GLFW_KEY_KP_8;
    case Key_KP_9:
        return GLFW_KEY_KP_9;
    case Key_KPDecimal:
        return GLFW_KEY_KP_DECIMAL;
    case Key_KPDivide:
        return GLFW_KEY_KP_DIVIDE;
    case Key_KPMultiply:
        return GLFW_KEY_KP_MULTIPLY;
    case Key_KPSubtract:
        return GLFW_KEY_KP_SUBTRACT;
    case Key_KPAdd:
        return GLFW_KEY_KP_ADD;
    case Key_KPEnter:
        return GLFW_KEY_KP_ENTER;
    case Key_KPEqual:
        return GLFW_KEY_KP_EQUAL;
    case Key_LeftShift:
        return GLFW_KEY_LEFT_SHIFT;
    case Key_LeftControl:
        return GLFW_KEY_LEFT_CONTROL;
    case Key_LeftAlt:
        return GLFW_KEY_LEFT_ALT;
    case Key_LeftSuper:
        return GLFW_KEY_LEFT_SUPER;
    case Key_RightShift:
        return GLFW_KEY_RIGHT_SHIFT;
    case Key_RightControl:
        return GLFW_KEY_RIGHT_CONTROL;
    case Key_RightAlt:
        return GLFW_KEY_RIGHT_ALT;
    case Key_RightSuper:
        return GLFW_KEY_RIGHT_SUPER;
    case Key_Menu:
        return GLFW_KEY_MENU;
    case Key_None:
        return GLFW_KEY_LAST + 1;
    case Key_Count:
        return GLFW_KEY_LAST + 1;
    }
    return GLFW_KEY_LAST + 1;
}

static i32 toGlfw(const Mouse mouse)
{
    switch (mouse)
    {
    case Mouse_Button1:
        return GLFW_MOUSE_BUTTON_1;
    case Mouse_Button2:
        return GLFW_MOUSE_BUTTON_2;
    case Mouse_Button3:
        return GLFW_MOUSE_BUTTON_3;
    case Mouse_Button4:
        return GLFW_MOUSE_BUTTON_4;
    case Mouse_Button5:
        return GLFW_MOUSE_BUTTON_5;
    case Mouse_Button6:
        return GLFW_MOUSE_BUTTON_6;
    case Mouse_Button7:
        return GLFW_MOUSE_BUTTON_7;
    case Mouse_Button8:
        return GLFW_MOUSE_BUTTON_8;
    case Mouse_ButtonLast:
        return GLFW_MOUSE_BUTTON_LAST;
    case Mouse_ButtonLeft:
        return GLFW_MOUSE_BUTTON_LEFT;
    case Mouse_ButtonRight:
        return GLFW_MOUSE_BUTTON_RIGHT;
    case Mouse_ButtonMiddle:
        return GLFW_MOUSE_BUTTON_MIDDLE;
    case Mouse_None:
        return GLFW_MOUSE_BUTTON_LAST + 1;
    case Mouse_Count:
        return GLFW_MOUSE_BUTTON_LAST + 1;
    }
    return GLFW_MOUSE_BUTTON_LAST + 1;
}

static Key toKey(const i32 key)
{
    switch (key)
    {
    case GLFW_KEY_SPACE:
        return Key_Space;
    case GLFW_KEY_APOSTROPHE:
        return Key_Apostrophe;
    case GLFW_KEY_COMMA:
        return Key_Comma;
    case GLFW_KEY_MINUS:
        return Key_Minus;
    case GLFW_KEY_PERIOD:
        return Key_Period;
    case GLFW_KEY_SLASH:
        return Key_Slash;
    case GLFW_KEY_0:
        return Key_N0;
    case GLFW_KEY_1:
        return Key_N1;
    case GLFW_KEY_2:
        return Key_N2;
    case GLFW_KEY_3:
        return Key_N3;
    case GLFW_KEY_4:
        return Key_N4;
    case GLFW_KEY_5:
        return Key_N5;
    case GLFW_KEY_6:
        return Key_N6;
    case GLFW_KEY_7:
        return Key_N7;
    case GLFW_KEY_8:
        return Key_N8;
    case GLFW_KEY_9:
        return Key_N9;
    case GLFW_KEY_SEMICOLON:
        return Key_Semicolon;
    case GLFW_KEY_EQUAL:
        return Key_Equal;
    case GLFW_KEY_A:
        return Key_A;
    case GLFW_KEY_B:
        return Key_B;
    case GLFW_KEY_C:
        return Key_C;
    case GLFW_KEY_D:
        return Key_D;
    case GLFW_KEY_E:
        return Key_E;
    case GLFW_KEY_F:
        return Key_F;
    case GLFW_KEY_G:
        return Key_G;
    case GLFW_KEY_H:
        return Key_H;
    case GLFW_KEY_I:
        return Key_I;
    case GLFW_KEY_J:
        return Key_J;
    case GLFW_KEY_K:
        return Key_K;
    case GLFW_KEY_L:
        return Key_L;
    case GLFW_KEY_M:
        return Key_M;
    case GLFW_KEY_N:
        return Key_N;
    case GLFW_KEY_O:
        return Key_O;
    case GLFW_KEY_P:
        return Key_P;
    case GLFW_KEY_Q:
        return Key_Q;
    case GLFW_KEY_R:
        return Key_R;
    case GLFW_KEY_S:
        return Key_S;
    case GLFW_KEY_T:
        return Key_T;
    case GLFW_KEY_U:
        return Key_U;
    case GLFW_KEY_V:
        return Key_V;
    case GLFW_KEY_W:
        return Key_W;
    case GLFW_KEY_X:
        return Key_X;
    case GLFW_KEY_Y:
        return Key_Y;
    case GLFW_KEY_Z:
        return Key_Z;
    case GLFW_KEY_LEFT_BRACKET:
        return Key_LeftBracket;
    case GLFW_KEY_BACKSLASH:
        return Key_Backslash;
    case GLFW_KEY_RIGHT_BRACKET:
        return Key_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT:
        return Key_GraveAccent;
    case GLFW_KEY_WORLD_1:
        return Key_World_1;
    case GLFW_KEY_WORLD_2:
        return Key_World_2;
    case GLFW_KEY_ESCAPE:
        return Key_Escape;
    case GLFW_KEY_ENTER:
        return Key_Enter;
    case GLFW_KEY_TAB:
        return Key_Tab;
    case GLFW_KEY_BACKSPACE:
        return Key_Backspace;
    case GLFW_KEY_INSERT:
        return Key_Insert;
    case GLFW_KEY_DELETE:
        return Key_Delete;
    case GLFW_KEY_RIGHT:
        return Key_Right;
    case GLFW_KEY_LEFT:
        return Key_Left;
    case GLFW_KEY_DOWN:
        return Key_Down;
    case GLFW_KEY_UP:
        return Key_Up;
    case GLFW_KEY_PAGE_UP:
        return Key_PageUp;
    case GLFW_KEY_PAGE_DOWN:
        return Key_PageDown;
    case GLFW_KEY_HOME:
        return Key_Home;
    case GLFW_KEY_END:
        return Key_End;
    case GLFW_KEY_CAPS_LOCK:
        return Key_CapsLock;
    case GLFW_KEY_SCROLL_LOCK:
        return Key_ScrollLock;
    case GLFW_KEY_NUM_LOCK:
        return Key_NumLock;
    case GLFW_KEY_PRINT_SCREEN:
        return Key_PrintScreen;
    case GLFW_KEY_PAUSE:
        return Key_Pause;
    case GLFW_KEY_F1:
        return Key_F1;
    case GLFW_KEY_F2:
        return Key_F2;
    case GLFW_KEY_F3:
        return Key_F3;
    case GLFW_KEY_F4:
        return Key_F4;
    case GLFW_KEY_F5:
        return Key_F5;
    case GLFW_KEY_F6:
        return Key_F6;
    case GLFW_KEY_F7:
        return Key_F7;
    case GLFW_KEY_F8:
        return Key_F8;
    case GLFW_KEY_F9:
        return Key_F9;
    case GLFW_KEY_F10:
        return Key_F10;
    case GLFW_KEY_F11:
        return Key_F11;
    case GLFW_KEY_F12:
        return Key_F12;
    case GLFW_KEY_F13:
        return Key_F13;
    case GLFW_KEY_F14:
        return Key_F14;
    case GLFW_KEY_F15:
        return Key_F15;
    case GLFW_KEY_F16:
        return Key_F16;
    case GLFW_KEY_F17:
        return Key_F17;
    case GLFW_KEY_F18:
        return Key_F18;
    case GLFW_KEY_F19:
        return Key_F19;
    case GLFW_KEY_F20:
        return Key_F20;
    case GLFW_KEY_F21:
        return Key_F21;
    case GLFW_KEY_F22:
        return Key_F22;
    case GLFW_KEY_F23:
        return Key_F23;
    case GLFW_KEY_F24:
        return Key_F24;
    case GLFW_KEY_F25:
        return Key_F25;
    case GLFW_KEY_KP_0:
        return Key_KP_0;
    case GLFW_KEY_KP_1:
        return Key_KP_1;
    case GLFW_KEY_KP_2:
        return Key_KP_2;
    case GLFW_KEY_KP_3:
        return Key_KP_3;
    case GLFW_KEY_KP_4:
        return Key_KP_4;
    case GLFW_KEY_KP_5:
        return Key_KP_5;
    case GLFW_KEY_KP_6:
        return Key_KP_6;
    case GLFW_KEY_KP_7:
        return Key_KP_7;
    case GLFW_KEY_KP_8:
        return Key_KP_8;
    case GLFW_KEY_KP_9:
        return Key_KP_9;
    case GLFW_KEY_KP_DECIMAL:
        return Key_KPDecimal;
    case GLFW_KEY_KP_DIVIDE:
        return Key_KPDivide;
    case GLFW_KEY_KP_MULTIPLY:
        return Key_KPMultiply;
    case GLFW_KEY_KP_SUBTRACT:
        return Key_KPSubtract;
    case GLFW_KEY_KP_ADD:
        return Key_KPAdd;
    case GLFW_KEY_KP_ENTER:
        return Key_KPEnter;
    case GLFW_KEY_KP_EQUAL:
        return Key_KPEqual;
    case GLFW_KEY_LEFT_SHIFT:
        return Key_LeftShift;
    case GLFW_KEY_LEFT_CONTROL:
        return Key_LeftControl;
    case GLFW_KEY_LEFT_ALT:
        return Key_LeftAlt;
    case GLFW_KEY_LEFT_SUPER:
        return Key_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT:
        return Key_RightShift;
    case GLFW_KEY_RIGHT_CONTROL:
        return Key_RightControl;
    case GLFW_KEY_RIGHT_ALT:
        return Key_RightAlt;
    case GLFW_KEY_RIGHT_SUPER:
        return Key_RightSuper;
    case GLFW_KEY_MENU:
        return Key_Menu;
    default:
        return Key_None;
    }
}

static Mouse toMouse(const i32 mouse)
{
    switch (mouse)
    {
    case GLFW_MOUSE_BUTTON_1:
        return Mouse_Button1;
    case GLFW_MOUSE_BUTTON_2:
        return Mouse_Button2;
    case GLFW_MOUSE_BUTTON_3:
        return Mouse_Button3;
    case GLFW_MOUSE_BUTTON_4:
        return Mouse_Button4;
    case GLFW_MOUSE_BUTTON_5:
        return Mouse_Button5;
    case GLFW_MOUSE_BUTTON_6:
        return Mouse_Button6;
    case GLFW_MOUSE_BUTTON_7:
        return Mouse_Button7;
    case GLFW_MOUSE_BUTTON_8:
        return Mouse_Button8;
        // case GLFW_MOUSE_BUTTON_LAST:
        //     return Mouse_ButtonLast;
        // case GLFW_MOUSE_BUTTON_LEFT:
        //     return Mouse_ButtonLeft;
        // case GLFW_MOUSE_BUTTON_RIGHT:
        //     return Mouse_ButtonRight;
        // case GLFW_MOUSE_BUTTON_MIDDLE:
        // return Mouse_ButtonMiddle;
    default:
        return Mouse_None;
    }
}

void windowMoveCallback(GLFWwindow *handle, const i32 x, const i32 y)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_WindowMoved;

    event.WindowPos = i32v2{x, y};

    window->PushEvent(event);
    window->UpdateMonitorDeltaTime(window->GetMonitorDeltaTime());
}

void windowSizeCallback(GLFWwindow *handle, const i32 width, const i32 height)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_WindowResized;

    event.WindowSize = u32v2{u32(width), u32(height)};

    window->ResetResizeClock();
    window->PushEvent(event);
}

static void framebufferSizeCallback(GLFWwindow *handle, const i32 width, const i32 height)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_FramebufferResized;

    event.WindowSize = u32v2{u32(width), u32(height)};

    window->ResetResizeClock();
    window->PushEvent(event);
}

static void windowFocusCallback(GLFWwindow *handle, const i32 focused)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = focused ? Event_WindowFocused : Event_WindowUnfocused;
    window->PushEvent(event);
}

static void windowCloseCallback(GLFWwindow *handle)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_WindowClosed;
    window->PushEvent(event);
}

static void windowIconifyCallback(GLFWwindow *handle, const i32 iconified)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = iconified ? Event_WindowMinimized : Event_WindowRestored;
    window->PushEvent(event);
}

static void keyCallback(GLFWwindow *handle, const i32 key, const i32, const i32 action, const i32)
{
    Event event{};
    Window *window = Window::FromHandle(handle);

    switch (action)
    {
    case GLFW_PRESS:
        event.Type = Event_KeyPressed;
        break;
    case GLFW_RELEASE:
        event.Type = Event_KeyReleased;
        break;
    case GLFW_REPEAT:
        event.Type = Event_KeyRepeat;
        break;
    default:
        return;
    }

    event.Key = toKey(key);
    window->PushEvent(event);
}

static void charCallback(GLFWwindow *handle, const u32 codepoint)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_CharInput;
    event.Character.Codepoint = codepoint;
    window->PushEvent(event);
}

static void cursorPositionCallback(GLFWwindow *handle, const f64 xpos, const f64 ypos)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_MouseMoved;
    event.Mouse.Position = f32v2{f32(xpos), f32(ypos)};
    window->PushEvent(event);
}

static void cursorEnterCallback(GLFWwindow *handle, const i32 entered)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = entered ? Event_MouseEntered : Event_MouseLeft;
    window->PushEvent(event);
}

static void mouseButtonCallback(GLFWwindow *handle, const i32 button, const i32 action, const i32)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = action == GLFW_PRESS ? Event_MousePressed : Event_MouseReleased;
    event.Mouse.Button = toMouse(button);
    window->PushEvent(event);
}

static void scrollCallback(GLFWwindow *handle, const f64 xoffset, const f64 yoffset)
{
    Event event{};
    Window *window = Window::FromHandle(handle);
    event.Type = Event_Scrolled;
    event.ScrollOffset = f32v2{f32(xoffset), f32(yoffset)};
    window->PushEvent(event);
}

static void installCallbacks(GLFWwindow *handle)
{
    glfwSetWindowPosCallback(handle, windowMoveCallback);
    glfwSetWindowSizeCallback(handle, windowSizeCallback);
    glfwSetFramebufferSizeCallback(handle, framebufferSizeCallback);
    glfwSetWindowFocusCallback(handle, windowFocusCallback);
    glfwSetWindowCloseCallback(handle, windowCloseCallback);
    glfwSetWindowIconifyCallback(handle, windowIconifyCallback);

    glfwSetKeyCallback(handle, keyCallback);
    glfwSetCharCallback(handle, charCallback);

    glfwSetCursorPosCallback(handle, cursorPositionCallback);
    glfwSetCursorEnterCallback(handle, cursorEnterCallback);
    glfwSetMouseButtonCallback(handle, mouseButtonCallback);
    glfwSetScrollCallback(handle, scrollCallback);
}

void Window::InstallCallbacks()
{
    installCallbacks(m_Window);
}

f32v2 Window::GetScreenMousePosition() const
{
    f64 xpos, ypos;
    glfwGetCursorPos(m_Window, &xpos, &ypos);
    const f32v2 dims = GetScreenDimensions();
    return f32v2{2.f * f32(xpos) / dims[0] - 1.f, 1.f - 2.f * f32(ypos) / dims[1]};
}

bool Window::IsKeyPressed(const Key key) const
{
    return glfwGetKey(m_Window, toGlfw(key)) == GLFW_PRESS;
}
bool Window::IsKeyReleased(const Key key) const
{
    return glfwGetKey(m_Window, toGlfw(key)) == GLFW_RELEASE;
}

bool Window::IsMousePressed(const Mouse button) const
{
    return glfwGetMouseButton(m_Window, toGlfw(button)) == GLFW_PRESS;
}
bool Window::IsMouseReleased(const Mouse button) const
{
    return glfwGetMouseButton(m_Window, toGlfw(button)) == GLFW_RELEASE;
}

// TODO(Isma): Use LookTowards
template <Dimension D>
void Window::ControlCamera(const TKit::Timespan deltaTime, Camera<D> *camera, const CameraControls<D> &controls) const
{
    Onyx::Transform<D> &view = camera->View;
    const f32 step = deltaTime.AsSeconds();
    f32v<D> translation{0.f};
    if (IsKeyPressed(controls.Left))
        translation[0] -= view.Scale[0] * step;
    if (IsKeyPressed(controls.Right))
        translation[0] += view.Scale[0] * step;

    if (IsKeyPressed(controls.Up))
        translation[1] += view.Scale[1] * step;
    if (IsKeyPressed(controls.Down))
        translation[1] -= view.Scale[1] * step;

    if constexpr (D == D2)
    {
        if (IsKeyPressed(controls.RotateLeft))
            view.Rotation += step;
        if (IsKeyPressed(controls.RotateRight))
            view.Rotation -= step;
    }
    else
    {
        if (IsKeyPressed(controls.Forward))
            translation[2] -= view.Scale[2] * step;
        if (IsKeyPressed(controls.Backward))
            translation[2] += view.Scale[2] * step;

        f32v2 mpos = GetScreenMousePosition();
        mpos[1] = -mpos[1]; // Invert y axis to undo onyx's inversion to GLFW, so that now when applying the
                            // rotation around x axis everything works out

        const bool lookAround = IsKeyPressed(controls.ToggleLookAround);
        const f32v2 delta = lookAround ? 3.f * (m_PrevMousePos - mpos) : f32v2{0.f};
        m_PrevMousePos = mpos;

        f32v3 angles{delta[1], delta[0], 0.f};
        if (IsKeyPressed(controls.RotateLeft))
            angles[2] += step;
        if (IsKeyPressed(controls.RotateRight))
            angles[2] -= step;

        view.Rotation *= f32q{angles};
    }

    const auto rmat = Onyx::Transform<D>::ComputeRotationMatrix(view.Rotation);
    view.Translation += rmat * translation;
}

template Result<RenderView<D2> *> Window::CreateRenderView<D2>(Camera<D2> *camera, RenderViewFlags flags,
                                                               const ScreenViewport &viewport,
                                                               const ScreenScissor &scissor);

template Result<RenderView<D3> *> Window::CreateRenderView<D3>(Camera<D3> *camera, RenderViewFlags flags,
                                                               const ScreenViewport &viewport,
                                                               const ScreenScissor &scissor);

template void Window::DestroyRenderView<D2>(RenderView<D2> *view);
template void Window::DestroyRenderView<D3>(RenderView<D3> *view);

template void Window::ControlCamera<D3>(const TKit::Timespan deltaTime, Camera<D3> *camera,
                                        const CameraControls<D3> &controls) const;

template void Window::ControlCamera<D2>(const TKit::Timespan deltaTime, Camera<D2> *camera,
                                        const CameraControls<D2> &controls) const;

} // namespace Onyx
