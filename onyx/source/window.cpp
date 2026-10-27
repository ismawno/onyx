#include "pch.hpp"
#include "onyx/window.hpp"
#include "onyx/overlay.hpp"
#include "onyx/camera.hpp"
#include "pass.hpp"
#include "graph/execution.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
// edge cases a bit unrealistic yes
u32 ToFrequency(const TKit::Timespan deltaTime)
{
    const f32 seconds = deltaTime.AsSeconds();
    if (Math::ApproachesZero(seconds))
        return TKIT_U32_MAX;
    if (seconds == TKIT_F32_MAX)
        return 0;

    return u32(1.f / deltaTime.AsSeconds()) + 1;
}
TKit::Timespan ToDeltaTime(const u32 frequency)
{
    if (frequency == 0)
        return TKit::Timespan{TKit::Timespan::Nanoseconds::max()};
    if (frequency == TKIT_U32_MAX)
        return TKit::Timespan{};
    return TKit::Timespan::FromSeconds(1.f / f32(frequency));
}

bool Window::IsFullScreen() const
{
    Graph::Monitor monitor = Graph::Window_GetMonitor(m_Window);
    if (monitor != NullHandle)
        return true;

    monitor = Graph::Monitor_GetPrimary();
    if (monitor == NullHandle)
        return false;

    const Graph::VideoMode mode = Graph::Monitor_GetVideoMode(monitor);
    const i32v2 mpos = Graph::Monitor_GetPosition(monitor);

    const u32v2 size = Graph::Window_GetScreenSize(m_Window);
    const i32v2 pos = GetPosition();

    return pos[0] == mpos[0] && pos[1] == mpos[1] && size[0] == mode.Size[0] && size[1] == mode.Size[1];
}
bool Window::IsHovered() const
{
    const f32v2 mpos = GetMousePosition();
    const f32v2 size = f32v2{GetSize()};

    return mpos[0] >= 0.f && mpos[0] < size[0] && mpos[1] >= 0.f && mpos[1] < size[1];
}

static Graph::WindowFlags toGraph(const WindowFlags flags)
{
    Graph::WindowFlags gflags = 0;
    if (flags & WindowFlag_NoResize)
        gflags |= Graph::WindowFlag_NoResize;
    if (flags & WindowFlag_Invisible)
        gflags |= Graph::WindowFlag_Invisible;
    if (flags & WindowFlag_NoDecoration)
        gflags |= Graph::WindowFlag_NoDecoration;
    if (flags & WindowFlag_NotFocused)
        gflags |= Graph::WindowFlag_NotFocused;
    if (flags & WindowFlag_Floating)
        gflags |= Graph::WindowFlag_Floating;
    if (flags & WindowFlag_FocusOnShow)
        gflags |= Graph::WindowFlag_FocusOnShow;
    if (flags & WindowFlag_Iconified)
        gflags |= Graph::WindowFlag_Iconified;
    return gflags;
}

static WindowFlags fromGraph(const Graph::WindowFlags gflags)
{
    WindowFlags flags = 0;
    if (gflags & Graph::WindowFlag_NoResize)
        flags |= WindowFlag_NoResize;
    if (gflags & Graph::WindowFlag_Invisible)
        flags |= WindowFlag_Invisible;
    if (gflags & Graph::WindowFlag_NoDecoration)
        flags |= WindowFlag_NoDecoration;
    if (gflags & Graph::WindowFlag_NotFocused)
        flags |= WindowFlag_NotFocused;
    if (gflags & Graph::WindowFlag_Floating)
        flags |= WindowFlag_Floating;
    if (gflags & Graph::WindowFlag_FocusOnShow)
        flags |= WindowFlag_FocusOnShow;
    if (gflags & Graph::WindowFlag_Iconified)
        flags |= WindowFlag_Iconified;
    return flags;
}

WindowFlags Window::GetFlags() const
{
    return fromGraph(Graph::Window_GetFlags(m_Window));
}
void Window::SetFlags(const WindowFlags flags)
{
    Graph::Window_SetFlags(m_Window, toGraph(flags));
}
void Window::AddFlags(const WindowFlags flags)
{
    Graph::Window_AddFlags(m_Window, toGraph(flags));
}
void Window::RemoveFlags(const WindowFlags flags)
{
    Graph::Window_RemoveFlags(m_Window, toGraph(flags));
}

void Window::waitForValidExtent() const
{
    Graph::PollEvents();
    u32v2 size = Graph::Window_GetPixelSize(m_Window);
    while (size[0] == 0 || size[1] == 0)
    {
        Graph::WaitEvents();
        size = Graph::Window_GetPixelSize(m_Window);
    }
}
Window::Window(const WindowSpecs &specs)
{
    TKIT_ASSERT(specs.ScreenSize[0] != 0 && specs.ScreenSize[1] != 0,
                "[ONYX][WINDOW] Cannot have window size of zero! Passed size: {}", specs.ScreenSize);

    const Graph::WindowFlags wflags = toGraph(specs.Flags) | Graph::WindowFlag_NoClientApi |
                                      Graph::WindowFlag_ScaleToMonitor | Graph::WindowFlag_ScaleFramebuffer;

    m_Window = Graph::Window_Create({.Title = specs.Title,
                                     .Position = specs.Position,
                                     .ScreenSize = specs.ScreenSize,
                                     .Flags = wflags,
                                     .UserData = this});

    m_PresentMode = specs.PresentMode;

    createSwapchain();
    UpdateMonitorDeltaTime();
    if (!(specs.Flags & WindowFlag_DoNotInstallCallbacks))
        InstallCallbacks();
}

Window::~Window()
{
    Graph::Swapchain_Destroy(m_Swapchain);

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const Overlay *ov : m_Overlays)
        tier->Destroy(ov);

    Graph::Window_Destroy(m_Window);
}

bool Window::handlePresentOrAcquireResult(const Graph::SwapchainStatus status, const bool imageAcquired)
{
    if (status == Graph::SwapchainStatus_SurfaceLost)
    {
        recreateSurface();
        return false;
    }

    if (imageAcquired && status != Graph::SwapchainStatus_OutOfDate)
        return true;

    if (m_MustRecreateSwapchain || status == Graph::SwapchainStatus_OutOfDate ||
        status == Graph::SwapchainStatus_Suboptimal)
    {
        recreateSwapchain();
        return false;
    }

    return true;
}

u32 Window::GetSwapchainImageCount() const
{
    return Graph::Swapchain_GetImageCount(m_Swapchain);
}

void Window::Present()
{
    TKIT_PROFILE_NSCOPE("Onyx::FrameScheduler::Present");
    const Graph::SwapchainStatus status = Graph::Swapchain_Present(m_Swapchain);
    handlePresentOrAcquireResult(status, false);
}

bool Window::AcquireNextImage(const u64 timeout)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::AcquireNextImage");

    const Graph::SwapchainStatus status = Graph::Swapchain_AcquireNextImage(m_Swapchain, timeout);
    if (status != Graph::SwapchainStatus_NotReady && handlePresentOrAcquireResult(status, true))
    {
        findAvailableFramebuffers();
        return true;
    }
    return false;
}

void Window::createSwapchain()
{
    Graph::SwapchainSpecs spc{};
    spc.SurfaceFormat = GetAttachmentFormat(AttachmentKind_LinearColor);
    spc.Window = m_Window;
    spc.OldSwapchain = m_Swapchain;
    spc.PresentMode = m_PresentMode;
    m_Swapchain = Graph::Swapchain_Create(spc);

    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-swapchain-window-'{}'", GetTitle());
        Graph::Swapchain_SetName(m_Swapchain, name.CString());
    }
}

void Window::recreateSwapchain()
{
    TKIT_LOG_DEBUG("[ONYX][WINDOW] Out of date swap chain. Re-creating swap chain and resources");

    const Graph::Swapchain old = m_Swapchain;
    waitForValidExtent();
    createSwapchain();
    Graph::Swapchain_Destroy(old);

    updateRenderViews();

    Event event{};
    event.Type = Event_SwapchainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;
}

void Window::recreateSurface()
{
    TKIT_LOG_WARNING("[ONYX][WINDOW] Surface lost... re-creating surface, swap chain and resources");

    Graph::Swapchain_Destroy(m_Swapchain);
    m_Swapchain = Graph::NullHandle;
    Graph::Window_RecreateSurface(m_Window);

    waitForValidExtent();
    createSwapchain();
    updateRenderViews();

    Event event{};
    event.Type = Event_SwapchainRecreated;
    PushEvent(event);

    m_MustRecreateSwapchain = false;
}

void Window::BeginRendering(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginRendering");

    const Graph::Image pimage = Graph::Swapchain_GetCurrentImage(m_Swapchain);

    Graph::ImageMemoryBarrierInfo binfo{};
    binfo.Image = pimage;
    binfo.NewLayout = Graph::ImageLayout_ColorAttachment;
    binfo.DstAccess = Graph::AccessFlag_ColorAttachmentWrite;
    binfo.SrcStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    binfo.DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    Graph::Command_ImageBarrier(cmd, binfo);

    Graph::AttachmentInfo att{};
    att.Layout = Graph::ImageLayout_ColorAttachment;
    att.LoadOp = Graph::LoadOp_Clear;
    att.StoreOp = Graph::StoreOp_Store;
    att.View = Graph::Image_GetView(pimage);
    att.ClearValue.Color = ClearColor.rgba;

    Graph::RenderPassInfo info{};
    info.RenderArea = {{0, 0}, Graph::Swapchain_GetExtent(m_Swapchain)};
    info.ColorAttachments = att;
    Graph::Command_BeginRenderPass(cmd, info);
}

void Window::EndRendering(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::EndRendering");

    Graph::Command_EndRenderPass(cmd);
    const Graph::Image pimage = Graph::Swapchain_GetCurrentImage(m_Swapchain);
    Graph::ImageMemoryBarrierInfo binfo{};
    binfo.Image = pimage;
    binfo.NewLayout = Graph::ImageLayout_Present;
    binfo.SrcAccess = Graph::AccessFlag_ColorAttachmentWrite;
    binfo.SrcStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    binfo.DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    Graph::Command_ImageBarrier(cmd, binfo);
}

TKit::Timespan Window::UpdateMonitorDeltaTime(const TKit::Timespan tdefault)
{
    Graph::Monitor monitor = Graph::Window_GetMonitor(m_Window);
    if (monitor == Graph::NullHandle)
        monitor = Graph::Monitor_GetPrimary();
    if (monitor == Graph::NullHandle)
    {
        m_MonitorDeltaTime = tdefault;
        return tdefault;
    }
    const Graph::VideoMode vm = Graph::Monitor_GetVideoMode(monitor);
    if (vm.RefreshRate == 0)
    {
        m_MonitorDeltaTime = tdefault;
        return tdefault;
    }
    m_MonitorDeltaTime = TKit::Timespan::FromSeconds(1.f / f32(vm.RefreshRate));
    return m_MonitorDeltaTime;
}

Overlay *Window::CreateOverlay()
{
    return CreateOverlay({});
}
Overlay *Window::CreateOverlay(const OverlaySpecs &specs)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Overlay *ov = tier->Create<Overlay>(this, specs);
    return m_Overlays.Append(ov);
}
void Window::DestroyOverlay(const Overlay *overlay)
{
    for (u32 i = 0; i < m_Overlays.GetSize(); ++i)
    {
        const Overlay *ov = m_Overlays[i];
        if (ov == overlay)
        {
            TKit::TierAllocator *tier = TKit::GetTier();
            tier->Destroy(ov);
            m_Overlays.RemoveUnordered(m_Overlays.begin() + i);
            return;
        }
    }
    TKIT_FATAL("[ONYX][WINDOW] Failed to find overlay to destroy. Ensure the overlay was created with the "
               "CreateOverlay() window function");
}

static void windowMoveCallback(const Graph::Window win, const i32v2 pos)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_WindowMoved;

    event.WindowPos = pos;

    window->PushEvent(event);
    window->UpdateMonitorDeltaTime(window->GetMonitorDeltaTime());
}

static void windowSizeCallback(const Graph::Window win, const u32v2)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_WindowResized;
    event.WindowSize = window->GetSize();

    window->RequestSwapchainRecreation();
    window->PushEvent(event);
}

static void contentScaleCallback(const Graph::Window win, const f32v2 scale)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_ContentScaleChanged;
    event.ContentScale = scale;
    event.WindowSize = window->GetSize();

    window->RequestSwapchainRecreation();
    window->PushEvent(event);
}

static void framebufferSizeCallback(const Graph::Window win, const u32v2)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_FramebufferResized;
    event.WindowSize = window->GetSize();

    window->RequestSwapchainRecreation();
    window->PushEvent(event);
}

static void windowFocusCallback(const Graph::Window win, const bool focused)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = focused ? Event_WindowFocused : Event_WindowUnfocused;
    window->PushEvent(event);
}

static void windowCloseCallback(const Graph::Window win)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_WindowClosed;
    window->PushEvent(event);
}

static void windowIconifyCallback(const Graph::Window win, const bool iconified)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = iconified ? Event_WindowMinimized : Event_WindowRestored;
    window->PushEvent(event);
}

static void keyCallback(const Graph::Window win, const Graph::Key key, const i32, const Graph::InputAction action,
                        const Graph::KeyModFlags)
{
    Event event{};
    Window *window = Window::FromHandle(win);

    switch (action)
    {
    case Graph::InputAction_Press:
        event.Type = Event_KeyPressed;
        break;
    case Graph::InputAction_Release:
        event.Type = Event_KeyReleased;
        break;
    case Graph::InputAction_Repeat:
        event.Type = Event_KeyRepeat;
        break;
    default:
        return;
    }

    event.Key = key;
    window->PushEvent(event);
}

static void charCallback(const Graph::Window win, const u32 code)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_CharInput;
    event.Character = code;
    window->PushEvent(event);
}

static void cursorPositionCallback(const Graph::Window win, const f32v2 pos)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_MouseMoved;
    event.Mouse.Position = pos;
    window->PushEvent(event);
}

static void cursorEnterCallback(const Graph::Window win, const bool entered)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = entered ? Event_MouseEntered : Event_MouseLeft;
    window->PushEvent(event);
}

static void mouseButtonCallback(const Graph::Window win, const Graph::Mouse button, const Graph::InputAction action,
                                const Graph::KeyModFlags)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = action == Graph::InputAction_Press ? Event_MousePressed : Event_MouseReleased;
    event.Mouse.Button = button;
    window->PushEvent(event);
}

static void scrollCallback(const Graph::Window win, const f32v2 offset)
{
    Event event{};
    Window *window = Window::FromHandle(win);
    event.Type = Event_Scrolled;
    event.ScrollOffset = offset;
    window->PushEvent(event);
}

void Window::InstallCallbacks()
{
    Graph::Window_Callback_Position(m_Window, windowMoveCallback);
    Graph::Window_Callback_Size(m_Window, windowSizeCallback);
    Graph::Window_Callback_ContentScale(m_Window, contentScaleCallback);
    Graph::Window_Callback_FramebufferSize(m_Window, framebufferSizeCallback);
    Graph::Window_Callback_Focus(m_Window, windowFocusCallback);
    Graph::Window_Callback_Close(m_Window, windowCloseCallback);
    Graph::Window_Callback_Iconify(m_Window, windowIconifyCallback);

    Graph::Window_Callback_Key(m_Window, keyCallback);
    Graph::Window_Callback_Char(m_Window, charCallback);

    Graph::Window_Callback_CursorPos(m_Window, cursorPositionCallback);
    Graph::Window_Callback_CursorEnter(m_Window, cursorEnterCallback);
    Graph::Window_Callback_MouseButton(m_Window, mouseButtonCallback);
    Graph::Window_Callback_Scroll(m_Window, scrollCallback);
}

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

        const f32v2 mpos = GetMousePosition() / f32v2{GetSize()};
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

template void Window::ControlCamera<D3>(const TKit::Timespan deltaTime, Camera<D3> *camera,
                                        const CameraControls<D3> &controls) const;

template void Window::ControlCamera<D2>(const TKit::Timespan deltaTime, Camera<D2> *camera,
                                        const CameraControls<D2> &controls) const;

} // namespace Onyx
