#pragma once

#include "onyx/dimension.hpp"
#include "onyx/input.hpp"
#include "onyx/color.hpp"
#include "onyx/core.hpp"
#include "onyx/render_target.hpp"
#include "tkit/profiling/clock.hpp"

ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE(SurfaceKHR)
ONYX_DECLARE_PLATFORM_HANDLES()

namespace VKit
{
class SwapChain;
class DeviceImage;
class Queue;
} // namespace VKit

namespace Onyx
{
enum MouseCursor : u8
{
    MouseCursor_Default,
    MouseCursor_Arrow,
    MouseCursor_NS,
    MouseCursor_EW,
    MouseCursor_NWSE,
    MouseCursor_NESW,
    MouseCursor_Hand,
    MouseCursor_CrossHair,
    MouseCursor_IBeam,
    MouseCursor_Count,
};

enum PresentMode : u8
{
    PresentMode_Immediate,
    PresentMode_Mailbox,
    PresentMode_VSync,
    PresentMode_Count
};

const char *ToString(PresentMode mode);

u32 ToFrequency(TKit::Timespan deltaTime);
TKit::Timespan ToDeltaTime(u32 frameRate);

template <Dimension D> struct Camera;

using Timeout = u64;

constexpr Timeout Block = TKIT_U64_MAX;
constexpr Timeout Poll = 0;

using WindowFlags = u16;
enum WindowFlagBit : WindowFlags
{
    WindowFlag_Resizable = 1U << 0,
    WindowFlag_Visible = 1U << 1,
    WindowFlag_Decorated = 1U << 2,
    WindowFlag_Focused = 1U << 3,
    WindowFlag_Floating = 1U << 4,
    WindowFlag_FocusOnShow = 1U << 5,
    WindowFlag_Iconified = 1U << 6,
    WindowFlag_InstallCallbacks = 1U << 7,
};

struct WindowSpecs
{
    const char *Title = "Onyx window";
    i32v2 Position{TKIT_I32_MAX}; // i32 max means let it be decided automatically
    u32v2 Dimensions{800, 600};
    PresentMode PresentMode = PresentMode_Immediate;
    WindowFlags Flags = WindowFlag_Resizable | WindowFlag_Visible | WindowFlag_Decorated | WindowFlag_Focused |
                        WindowFlag_InstallCallbacks;
};

struct WindowSyncData;

class Window final : public RenderTarget
{
  public:
    Window(const WindowSpecs &specs);
    ~Window();

    static Window *FromHandle(Onyx_WindowHandle *window);

    void MarkImageSemaphoreInUse(const Execution::Tracker &tracker);

    void BeginRendering(Onyx_CommandBuffer commandBuffer);
    void EndRendering(Onyx_CommandBuffer commandBuffer);

    bool ShouldClose() const;

    const Onyx_WindowHandle *GetHandle() const
    {
        return m_Window;
    }
    Onyx_WindowHandle *GetHandle()
    {
        return m_Window;
    }

    void EnableVSync()
    {
        SetPresentMode(PresentMode_VSync);
    }
    void DisableVSync(const PresentMode presentMode = PresentMode_Immediate)
    {
        SetPresentMode(presentMode);
    }
    bool IsVSync() const
    {
        return m_PresentMode == PresentMode_VSync;
    }

    void Show();
    void Focus();
    void ResetResizeClock()
    {
        m_TimeSinceResize.Restart();
    }

    bool CanQueryOpacity() const;

    const char *GetTitle() const;
    i32v2 GetPosition() const;
    u32v2 GetScreenDimensions() const;
    u32v2 GetPixelDimensions() const;
    f32 GetAspect() const;
    f32 GetOpacity() const;
    WindowFlags GetFlags() const;

    void SetTitle(const char *title);
    void SetPosition(const i32v2 &pos);
    void SetScreenDimensions(const u32v2 &dim);
    void SetAspect(u32 numer, u32 denom);
    void SetFlags(WindowFlags flags);
    void AddFlags(WindowFlags flags);
    void RemoveFlags(WindowFlags flags);
    void SetOpacity(f32 opacity);

    void SetMouseCursor(MouseCursor cursor);

    void InstallCallbacks();

    f32v2 GetNormalizedMousePosition() const;
    f32v2 GetAbsoluteMousePosition() const;

    bool IsKeyPressed(Key key) const;
    bool IsKeyReleased(Key key) const;

    bool IsMousePressed(Mouse button) const;
    bool IsMouseReleased(Mouse button) const;

    Onyx_SurfaceKHR GetSurface() const
    {
        return m_Surface;
    }

    /**
     * @brief Update the value of the delta time this window is currently in.
     *
     * This method gets called automatically and is exposed only if needed for some particular reason. If the window is
     * not fullscreen (and thus not associated with a monitor), the primary monitor will be used.
     *
     * @param tdefault If no monitor is found, a user provided default.
     * @return The delta time of the monitor.
     */
    TKit::Timespan UpdateMonitorDeltaTime(TKit::Timespan tdefault = TKit::Timespan::FromSeconds(1.f / 60.f));

    TKit::Timespan GetMonitorDeltaTime() const
    {
        return m_MonitorDeltaTime;
    }

    void FlagShouldClose();
    void PushEvent(const Event &event);

    const TKit::TierArray<Event> &GetNewEvents() const
    {
        return m_Events;
    }

    void FlushEvents();

    template <Dimension D> RenderView<D> *GetMouseRenderView() const
    {
        const TKit::TierArray<RenderView<D> *> &rvs = getSortedViews<D>();
        const f32v2 mpos = GetNormalizedMousePosition();

        for (u32 i = rvs.GetSize() - 1; i < rvs.GetSize(); --i)
            if (rvs[i]->IsWithinViewport(mpos))
                return rvs[i];

        return nullptr;
    }

    RenderTargetInfo CreateRenderTargetInfo() override
    {
        RenderTargetInfo info;
        info.Views2 = getSortedViews<D2>();
        info.Views3 = getSortedViews<D3>();
        info.ImageAvailableSemaphore = GetImageAvailableSemaphore();
        info.RenderFinishedSemaphore = GetRenderFinishedSemaphore();
        return info;
    }

    template <Dimension D>
    void ControlCamera(TKit::Timespan deltaTime, Camera<D> *camera, const CameraControls<D> &controls = {}) const;

    bool AcquireNextImage(Timeout timeout);
    void Present();

    void RequestSwapchainRecreation()
    {
        m_MustRecreateSwapchain = true;
    }

    u32 GetSwapChainImageCount() const;

    PresentMode GetPresentMode() const
    {
        return m_PresentMode;
    }

    void SetPresentMode(const PresentMode presentMode)
    {
        if (m_PresentMode == presentMode)
            return;
        m_MustRecreateSwapchain = true;
        m_PresentMode = presentMode;
    }

    Onyx_Semaphore GetImageAvailableSemaphore() const;
    Onyx_Semaphore GetRenderFinishedSemaphore() const;

  private:
    u32v2 getExtent() const override;
    void extractSwapChainImages();

    void createSwapChain(const u32v2 &windowExtent);
    void createSyncData();
    void destroySyncData();
    void drainWork();
    void recreateSwapChain();
    void recreateResources();
    void recreateSurface();
    bool handlePresentOrAcquireResult(const u32 result);
    void nameSurface();
    void nameSwapChain();
    void nameSyncData();
    void nameSwapChainImages();

    u32v2 getNewExtent();

    Onyx_WindowHandle *m_Window;

    TKit::FixedArray<Onyx_CursorHandle *, MouseCursor_Count> m_Cursors{};

    TKit::TierArray<Event> m_Events;
    Onyx_SurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};
    TKit::Clock m_TimeSinceResize{};

    VKit::SwapChain *m_SwapChain = nullptr;
    TKit::TierArray<VKit::DeviceImage *> m_Presentation{};
    TKit::TierArray<WindowSyncData *> m_SyncData{};

    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_SyncIndex = 0;
    mutable f32v2 m_PrevMousePos{0.f};

    PresentMode m_PresentMode;
    bool m_MustRecreateSwapchain = false;
};
} // namespace Onyx
