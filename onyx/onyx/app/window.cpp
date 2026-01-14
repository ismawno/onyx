#include "onyx/core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/profiling/vulkan.hpp"
#include "tkit/utils/debug.hpp"
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
    TKIT_ASSERT(s_ViewCache != 0, "[ONYX] Maximum number of windows exceeded!");
    const u32 index = static_cast<u32>(std::countr_zero(s_ViewCache));
    const u64 viewBit = 1 << index;
    s_ViewCache &= ~viewBit;
    return viewBit;
}
static void deallocateViewBit(const u64 p_ViewBit)
{
    s_ViewCache |= p_ViewBit;
}

// edge cases a bit unrealistic yes
u32 ToFrequency(const TKit::Timespan p_DeltaTime)
{
    const f32 seconds = p_DeltaTime.AsSeconds();
    if (TKit::ApproachesZero(seconds))
        return TKIT_U32_MAX;
    if (seconds == TKIT_F32_MAX)
        return 0;

    return static_cast<u32>(1.f / p_DeltaTime.AsSeconds()) + 1;
}
TKit::Timespan ToDeltaTime(const u32 p_Frequency)
{
    if (p_Frequency == 0)
        return TKit::Timespan::FromSeconds(TKIT_F32_MAX);
    if (p_Frequency == TKIT_U32_MAX)
        return TKit::Timespan{};
    return TKit::Timespan::FromSeconds(1.f / static_cast<f32>(p_Frequency));
}

static TKit::BlockAllocator createAllocator()
{
    constexpr u32 size2 = static_cast<u32>(sizeof(Camera<D2>));
    constexpr u32 size3 = static_cast<u32>(sizeof(Camera<D3>));

    constexpr u32 align2 = static_cast<u32>(alignof(Camera<D2>));
    constexpr u32 align3 = static_cast<u32>(alignof(Camera<D3>));

    constexpr u32 size = Math::Max(size2, size3);
    constexpr u32 alignment = Math::Max(align2, align3);

    return TKit::BlockAllocator{size * 2 * MaxCameras, size, alignment};
}

Window::Window() : Window(Specs{})
{
}

Window::Window(const Specs &p_Specs)
    : m_Allocator(createAllocator()), m_Name(p_Specs.Name), m_Dimensions(p_Specs.Dimensions),
      m_PresentMode(p_Specs.PresentMode), m_Flags(p_Specs.Flags)
{
    TKIT_LOG_DEBUG("[ONYX] Window '{}' has been instantiated", p_Specs.Name);
    m_ViewBit = allocateViewBit();
    createWindow(p_Specs);

    const VkExtent2D extent = waitGlfwEvents();
    createSwapChain(extent);
    m_SyncData = Execution::CreateSyncData(m_SwapChain.GetImageCount());
    m_Images = createImageData();
#ifdef TKIT_ENABLE_INSTRUMENTATION
    m_ColorIndex = s_ColorIndex++ & 3;
#endif
    UpdateMonitorDeltaTime();
    m_Present = Execution::FindSuitableQueue(VKit::Queue_Present);
}

Window::~Window()
{
    TKIT_LOG_DEBUG("[ONYX] Window '{}' is about to be destroyed", m_Name);
    deallocateViewBit(m_ViewBit);
    Renderer::ClearWindow(this);
    Core::DeviceWaitIdle();
    destroyImageData();
    Execution::DestroySyncData(m_SyncData);

    m_SwapChain.Destroy();
    for (Camera<D2> *context : m_Cameras2)
        m_Allocator.Destroy(context);
    for (Camera<D3> *context : m_Cameras3)
        m_Allocator.Destroy(context);

    Core::GetInstanceTable().DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow(const Specs &p_Specs)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Flags & WindowFlag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Flags & WindowFlag_Visible);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Flags & WindowFlag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Flags & WindowFlag_Focused);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Flags & WindowFlag_Floating);

    m_Window = glfwCreateWindow(static_cast<i32>(p_Specs.Dimensions[0]), static_cast<i32>(p_Specs.Dimensions[1]),
                                p_Specs.Name, nullptr, nullptr);
    TKIT_ASSERT(m_Window, "[ONYX] Failed to create a GLFW window");

    if (p_Specs.Position != u32v2{TKIT_U32_MAX})
    {
        glfwSetWindowPos(m_Window, static_cast<i32>(p_Specs.Position[0]), static_cast<i32>(p_Specs.Position[1]));
        m_Position = p_Specs.Position;
    }
    else
    {
        i32 x, y;
        glfwGetWindowPos(m_Window, &x, &y);
        m_Position = u32v2{x, y};
    }

    VKIT_CHECK_EXPRESSION(glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface));
    glfwSetWindowUserPointer(m_Window, this);

    if (!Core::IsDeviceCreated())
        Core::CreateDevice(m_Surface);
    Input::InstallCallbacks(*this);
}

bool Window::handleImageResult(const VkResult p_Result)
{
    if (p_Result == VK_NOT_READY || p_Result == VK_TIMEOUT)
        return false;

    if (p_Result == VK_ERROR_SURFACE_LOST_KHR)
    {
        recreateSurface();
        return false;
    }

    const bool needRecreation =
        p_Result == VK_ERROR_OUT_OF_DATE_KHR || p_Result == VK_SUBOPTIMAL_KHR || m_MustRecreateSwapchain;

    TKIT_ASSERT(needRecreation || p_Result == VK_SUCCESS || p_Result == VK_ERROR_SURFACE_LOST_KHR,
                "[ONYX] Failed to submit command buffers. The vulkan error is the following: {}",
                VKit::ResultToString(p_Result));

    if (needRecreation)
    {
        recreateSwapChain();
        return false;
    }
    return true;
}

void Window::Present(const Renderer::RenderSubmitInfo &p_Info)
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

    const auto &table = Core::GetDeviceTable();
    const VkResult result = table.QueuePresentKHR(*m_Present, &presentInfo);
    handleImageResult(result);

    // a bit random that 0
    m_LastGraphicsSubmission.Timeline = p_Info.SignalSemaphores[0].semaphore;
    m_LastGraphicsSubmission.InFlightValue = p_Info.SignalSemaphores[0].value;
}

bool Window::AcquireNextImage(const Timeout p_Timeout)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::AcquireNextImage");
    const auto &table = Core::GetDeviceTable();
    const auto &device = Core::GetDevice();
    const auto acquire = [&]() {
        const VkResult result =
            table.AcquireNextImageKHR(device, m_SwapChain, p_Timeout, m_SyncData[m_ImageIndex].ImageAvailableSemaphore,
                                      VK_NULL_HANDLE, &m_ImageIndex);
        return handleImageResult(result);
    };
    if (p_Timeout != Block)
        return acquire();

    if (m_LastGraphicsSubmission.Timeline)
    {
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_LastGraphicsSubmission.Timeline;
        waitInfo.pValues = &m_LastGraphicsSubmission.InFlightValue;
        m_LastGraphicsSubmission.Timeline = VK_NULL_HANDLE;
        VKIT_CHECK_EXPRESSION(table.WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
        return acquire();
    }
    if (!m_HasImage)
    {
        m_HasImage = true;
        return acquire();
    }
    return true;
}

VkExtent2D Window::waitGlfwEvents()
{
    VkExtent2D windowExtent = {GetScreenWidth(), GetScreenHeight()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {GetScreenWidth(), GetScreenHeight()};
        glfwWaitEvents();
    }
    return windowExtent;
}

void Window::createSwapChain(const VkExtent2D &p_WindowExtent)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto result = VKit::SwapChain::Builder(&device, GetSurface())
                            .RequestSurfaceFormat(SurfaceFormat)
                            .RequestPresentMode(m_PresentMode)
                            .RequestExtent(p_WindowExtent)
                            .RequestImageCount(3)
                            .SetOldSwapChain(m_SwapChain)
                            .AddFlags(VKit::SwapChainBuilderFlag_Clipped | VKit::SwapChainBuilderFlag_CreateImageViews)
                            .Build();

    VKIT_CHECK_RESULT(result);
    m_SwapChain = result.GetValue();
#ifdef TKIT_ENABLE_DEBUG_LOGS
    const u32 icount = m_SwapChain.GetImageCount();
    TKIT_LOG_DEBUG("[ONYX] Created swapchain with {} images", icount);
#endif
}

void Window::recreateSwapChain()
{
    TKIT_LOG_DEBUG("[ONYX] Out of date swap chain. Re-creating swap chain and resources");
    m_MustRecreateSwapchain = false;
    const VkExtent2D extent = waitGlfwEvents();
    Core::DeviceWaitIdle();

    VKit::SwapChain old = m_SwapChain;
    createSwapChain(extent);
    old.Destroy();
    recreateResources();
    adaptCamerasToViewportAspect();

    Event event{};
    event.Type = Event_SwapChainRecreated;
    PushEvent(event);
}

void Window::recreateSurface()
{
    TKIT_LOG_WARNING("[ONYX] Surface lost... re-creating surface, swap chain and resources");
    m_MustRecreateSwapchain = false;
    const VkExtent2D extent = waitGlfwEvents();
    Core::DeviceWaitIdle();

    m_SwapChain.Destroy();
    m_SwapChain = VKit::SwapChain{};

    Core::GetInstanceTable().DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    VKIT_CHECK_EXPRESSION(glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface));

    createSwapChain(extent);
    recreateResources();
}

void Window::recreateResources()
{
    destroyImageData();
    m_Images = createImageData();

    Execution::DestroySyncData(m_SyncData);
    m_SyncData = Execution::CreateSyncData(m_SwapChain.GetImageCount());
    m_ImageIndex = 0;
}

PerImageData<Window::ImageData> Window::createImageData()
{
    const VKit::SwapChain::Info &info = m_SwapChain.GetInfo();
    PerImageData<ImageData> images{};
    for (u32 i = 0; i < m_SwapChain.GetImageCount(); ++i)
    {
        ImageData data{};
        data.Presentation = &m_SwapChain.GetImage(i);

        const auto iresult =
            VKit::DeviceImage::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), info.Extent, DepthStencilFormat,
                                       VKit::DeviceImageFlag_DepthAttachment | VKit::DeviceImageFlag_StencilAttachment)
                .WithImageView()
                .Build();

        VKIT_CHECK_RESULT(iresult);
        data.DepthStencil = iresult.GetValue();

        images.Append(data);
    }
    return images;
}

void Window::destroyImageData()
{
    for (ImageData &data : m_Images)
        data.DepthStencil.Destroy();
}

void Window::BeginRendering(const VkCommandBuffer p_CommandBuffer, const Color &p_ClearColor)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginRendering");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    const auto &table = Core::GetDeviceTable();

    VkRenderingAttachmentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    present.imageView = m_Images[m_ImageIndex].Presentation->GetImageView();
    present.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    present.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    present.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    present.clearValue.color = {
        {p_ClearColor.RGBA[0], p_ClearColor.RGBA[1], p_ClearColor.RGBA[2], p_ClearColor.RGBA[3]}};

    m_Images[m_ImageIndex].Presentation->TransitionLayout(p_CommandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

    m_Images[m_ImageIndex].DepthStencil.TransitionLayout(p_CommandBuffer,
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

    table.CmdSetViewport(p_CommandBuffer, 0, 1, &viewport);
    table.CmdSetScissor(p_CommandBuffer, 0, 1, &scissor);

    table.CmdBeginRenderingKHR(p_CommandBuffer, &renderInfo);
}

void Window::EndRendering(const VkCommandBuffer p_CommandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::EndRendering");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    const auto &table = Core::GetDeviceTable();

    table.CmdEndRenderingKHR(p_CommandBuffer);
    m_Images[m_ImageIndex].Presentation->TransitionLayout(p_CommandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                          {.SrcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                           .DstAccess = 0,
                                                           .SrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                           .DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT});
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

TKit::Timespan Window::UpdateMonitorDeltaTime(const TKit::Timespan p_Default)
{
    GLFWmonitor *monitor = glfwGetWindowMonitor(m_Window);
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        m_MonitorDeltaTime = p_Default;
        return p_Default;
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    m_MonitorDeltaTime = TKit::Timespan::FromSeconds(1.f / static_cast<f32>(mode->refreshRate));
    return m_MonitorDeltaTime;
}

void Window::FlagShouldClose()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PushEvent(const Event &p_Event)
{
    if (!m_Events.IsFull())
        m_Events.Append(p_Event);
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
