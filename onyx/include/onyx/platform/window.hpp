#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/platform/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/view.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/container/static_array.hpp"

ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE(VkSurfaceKHR)

struct GLFWwindow;

namespace VKit
{
class SwapChain;
class DeviceImage;
class Queue;
} // namespace VKit

namespace Onyx
{
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
    WindowFlag_Resizable = 1 << 0,
    WindowFlag_Visible = 1 << 1,
    WindowFlag_Decorated = 1 << 2,
    WindowFlag_Focused = 1 << 3,
    WindowFlag_Floating = 1 << 4,
    WindowFlag_FocusOnShow = 1 << 5,
    WindowFlag_Iconified = 1 << 6,
    WindowFlag_InstallCallbacks = 1 << 7,
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

class Window
{
    TKIT_NON_COPYABLE(Window)
  public:
    Window(const WindowSpecs &specs);
    ~Window();

    static Window *FromHandle(GLFWwindow *window);

    void BeginRendering(VkCommandBuffer commandBuffer, const Execution::Tracker &tracker);
    void EndRendering(VkCommandBuffer commandBuffer);

    bool ShouldClose() const;

    const GLFWwindow *GetHandle() const
    {
        return m_Window;
    }
    GLFWwindow *GetHandle()
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

    void InstallCallbacks();

    f32v2 GetScreenMousePosition() const;

    bool IsKeyPressed(Key key) const;
    bool IsKeyReleased(Key key) const;

    bool IsMousePressed(Mouse button) const;
    bool IsMouseReleased(Mouse button) const;

    VkSurfaceKHR GetSurface() const
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

    template <Dimension D>
    RenderView<D> *CreateRenderView(Camera<D> *camera, RenderViewFlags flags = 0, const ScreenViewport &viewport = {},
                                    const ScreenScissor &scissor = {});

    template <Dimension D> void DestroyRenderView(RenderView<D> *rv);

    template <Dimension D> void BringToTop(RenderView<D> *rv)
    {
        rv->Layer = m_LayerIncrease++;
    }
    template <Dimension D> void BringToBottom(RenderView<D> *rv)
    {
        rv->Layer = --m_LayerDecrease;
    }

    template <Dimension D> RenderView<D> *GetMouseRenderView() const
    {
        const TKit::TierArray<RenderView<D> *> &rvs = getSortedViews<D>();
        const f32v2 mpos = GetScreenMousePosition();

        for (u32 i = rvs.GetSize() - 1; i < rvs.GetSize(); --i)
            if (rvs[i]->IsWithinViewport(mpos))
                return rvs[i];

        return nullptr;
    }

    RenderTargetInfo CreateRenderTargetInfo()
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

    VkSemaphore GetImageAvailableSemaphore() const;
    VkSemaphore GetRenderFinishedSemaphore() const;

    template <Dimension D> const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &GetRenderViews() const
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

    Color ClearColor = Color_Black;

  private:
    void updateRenderViews();
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

    // TODO(Isma): Implement sort here somehow. rename this to get sorted views
    template <Dimension D> TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> getSortedViews() const
    {
        TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> views = GetRenderViews<D>();
        std::sort(views.begin(), views.end(),
                  [](const RenderView<D> *rv1, const RenderView<D> *rv2) { return rv1->Layer < rv2->Layer; });
        return views;
    }

    template <Dimension D> TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &getRenderViews()
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

    GLFWwindow *m_Window;

    TKit::StaticArray<RenderView<D2> *, ONYX_MAX_VIEWS> m_RenderViews2{};
    TKit::StaticArray<RenderView<D3> *, ONYX_MAX_VIEWS> m_RenderViews3{};

    TKit::TierArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};
    TKit::Clock m_TimeSinceResize{};

    VKit::SwapChain *m_SwapChain = nullptr;
    TKit::TierArray<VKit::DeviceImage *> m_Presentation{};
    TKit::TierArray<WindowSyncData *> m_SyncData{};

    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_SyncIndex = 0;
    u32 m_LayerIncrease = TKIT_U32_MAX / 2;
    u32 m_LayerDecrease = TKIT_U32_MAX / 2;
    mutable f32v2 m_PrevMousePos{0.f};

    PresentMode m_PresentMode;
    bool m_MustRecreateSwapchain = false;
};
} // namespace Onyx
