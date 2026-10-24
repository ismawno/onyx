#pragma once

#include "onyx/dimension.hpp"
#include "onyx/core.hpp"
#include "onyx/render_target.hpp"
#include "graph/platform.hpp"
#include "graph/swap_chain.hpp"
#include "tkit/profiling/clock.hpp"

namespace Onyx
{
enum EventType : u8
{
    Event_KeyPressed,
    Event_KeyReleased,
    Event_KeyRepeat,
    Event_MouseMoved,
    Event_MousePressed,
    Event_MouseReleased,
    Event_MouseEntered,
    Event_MouseLeft,
    Event_Scrolled,
    Event_WindowMoved,
    Event_WindowResized,
    Event_WindowFocused,
    Event_WindowUnfocused,
    Event_WindowClosed,
    Event_WindowMinimized,
    Event_WindowRestored,
    Event_FramebufferResized,
    Event_ContentScaleChanged,
    Event_CharInput,
    Event_SwapchainRecreated,
    Event_Count,
};
struct Event
{
    struct MouseState
    {
        f32v2 Position{0.f};
        Graph::Mouse Button;
    };

    bool Empty = false;
    EventType Type;
    Graph::Key Key;

    i32v2 WindowPos{0};
    u32v2 WindowSize{0};

    MouseState Mouse;
    CodePoint Character;
    f32v2 ScrollOffset{0.f};

    f32v2 ContentScale;

    operator bool() const
    {
        return !Empty;
    }
};

class Overlay;
struct OverlaySpecs;

u32 ToFrequency(TKit::Timespan deltaTime);
TKit::Timespan ToDeltaTime(u32 frameRate);

template <Dimension D> struct Camera;

constexpr u64 Block = TKIT_U64_MAX;
constexpr u64 Poll = 0;

using WindowFlags = u16;
enum WindowFlagBit : WindowFlags
{
    WindowFlag_NoResize = 1U << 0,
    WindowFlag_Invisible = 1U << 1,
    WindowFlag_NoDecoration = 1U << 2,
    WindowFlag_NotFocused = 1U << 3,
    WindowFlag_Floating = 1U << 4,
    WindowFlag_FocusOnShow = 1U << 5,
    WindowFlag_Iconified = 1U << 6,
    WindowFlag_DoNotInstallCallbacks = 1U << 7,
};

struct WindowSpecs
{
    const char *Title = "Onyx window";
    i32v2 Position{TKIT_I32_MAX}; // i32 max means let it be decided automatically
    u32v2 ScreenSize{800, 600};
    Graph::PresentMode PresentMode = Graph::PresentMode_Immediate;
    WindowFlags Flags = 0;
};

struct WindowSyncData;

class Window final : public RenderTarget
{
  public:
    Window(const WindowSpecs &specs);
    ~Window();

    static Window *FromHandle(const Graph::Window win)
    {
        return scast<Window *>(Graph::Window_GetUserData(win));
    }

    bool ShouldClose() const
    {
        return Graph::Window_ShouldClose(m_Window);
    }
    Graph::Monitor GetMonitor() const
    {
        return Graph::Window_GetMonitor(m_Window);
    }

    void Show()
    {
        Graph::Window_Show(m_Window);
    }
    void Focus()
    {
        Graph::Window_Focus(m_Window);
    }

    const char *GetTitle() const
    {
        return Graph::Window_GetTitle(m_Window);
    }
    i32v2 GetPosition() const
    {
        return Graph::Window_GetPosition(m_Window);
    }
    u32v2 GetSize() const
    {
        const f32v2 pixels = f32v2{Graph::Window_GetPixelSize(m_Window)};
        return u32v2{pixels / Graph::Window_GetContentScale(m_Window)};
    }
    f32 GetAspect() const
    {
        return Graph::Window_GetAspect(m_Window);
    }
    f32 GetOpacity() const
    {
        return Graph::Window_GetOpacity(m_Window);
    }

    void SetTitle(const char *title)
    {
        Graph::Window_SetTitle(m_Window, title);
    }
    void SetPosition(const i32v2 &pos)
    {
        Graph::Window_SetPosition(m_Window, pos);
    }
    void SetSize(const u32v2 &size)
    {
        // we convert to logical. parameter "size" is also logical. in different OS, screen size can be logical or
        // pixel (physical). this factor is 1 when the former, and bigger than one in case screen is pixel, and size
        // must transition from logical to physical pixels
        const f32v2 factor = GetContentScale() * f32v2{Graph::Window_GetScreenSize(m_Window)} /
                             f32v2{Graph::Window_GetPixelSize(m_Window)};
        Graph::Window_SetScreenSize(m_Window, ApplyScale(size, factor));
    }
    void SetAspect(const u32 numer, const u32 denom)
    {
        Graph::Window_SetAspect(m_Window, numer, denom);
    }
    void SetOpacity(const f32 opacity)
    {
        Graph::Window_SetOpacity(m_Window, opacity);
    }

    void SetMouseCursor(const Graph::MouseCursor cursor)
    {
        Graph::Window_SetMouseCursor(m_Window, cursor);
    }

    f32v2 GetMousePosition() const
    {
        const f32v2 normalized =
            Graph::Window_GetCursorScreenPosition(m_Window) / f32v2{Graph::Window_GetScreenSize(m_Window)};
        return normalized * f32v2{GetSize()};
    }

    bool IsKeyPressed(const Graph::Key key) const
    {
        return Graph::Window_IsKeyPressed(m_Window, key);
    }
    bool IsKeyReleased(const Graph::Key key) const
    {
        return Graph::Window_IsKeyReleased(m_Window, key);
    }

    bool IsMousePressed(const Graph::Mouse button) const
    {
        return Graph::Window_IsMousePressed(m_Window, button);
    }
    bool IsMouseReleased(const Graph::Mouse button) const
    {
        return Graph::Window_IsMouseReleased(m_Window, button);
    }

    void PushEvent(const Event &event)
    {
        m_Events.Append(event);
    }
    void FlushEvents()
    {
        m_Events.Clear();
    }

    WindowFlags GetFlags() const;
    void SetFlags(WindowFlags flags);
    void AddFlags(WindowFlags flags);
    void RemoveFlags(WindowFlags flags);

    void BeginRendering(Graph::CommandBuffer cmd);
    void EndRendering(Graph::CommandBuffer cmd);

    Graph::Window GetHandle() const
    {
        return m_Window;
    }

    bool IsFullScreen() const;
    bool IsHovered() const;

    void InstallCallbacks();

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

    const TKit::TierArray<Event> &GetNewEvents() const
    {
        return m_Events;
    }

    template <Dimension D> RenderView<D> *GetMouseRenderView() const
    {
        const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> rvs = GetSortedViews<D>();
        const f32v2 mpos = GetMousePosition();

        for (u32 i = rvs.GetSize() - 1; i < rvs.GetSize(); --i)
            if (rvs[i]->IsWithinViewport(mpos))
                return rvs[i];

        return nullptr;
    }

    RenderTargetInfo CreateRenderTargetInfo() override
    {
        RenderTargetInfo info;
        info.Views2 = GetSortedViews<D2>();
        info.Views3 = GetSortedViews<D3>();
        info.Swapchain = m_Swapchain;
        return info;
    }

    Overlay *CreateOverlay();
    Overlay *CreateOverlay(const OverlaySpecs &specs);
    void DestroyOverlay(const Overlay *overlay);

    template <Dimension D>
    void ControlCamera(TKit::Timespan deltaTime, Camera<D> *camera, const CameraControls<D> &controls = {}) const;

    bool AcquireNextImage(u64 timeout);
    void Present();

    void RequestSwapchainRecreation()
    {
        m_MustRecreateSwapchain = true;
    }

    u32 GetSwapchainImageCount() const;
    Graph::Swapchain GetSwapchain() const
    {
        return m_Swapchain;
    }

    Graph::PresentMode GetPresentMode() const
    {
        return m_PresentMode;
    }

    void SetPresentMode(const Graph::PresentMode presentMode)
    {
        if (m_PresentMode == presentMode)
            return;
        m_MustRecreateSwapchain = true;
        m_PresentMode = presentMode;
    }

    f32v2 GetContentScale() const
    {
        return Graph::Window_GetContentScale(m_Window);
    }

  private:
    u32v2 getSize() const override
    {
        return GetSize();
    }
    f32v2 getContentScale() const override
    {
        return GetContentScale();
    }

    void createSwapchain();
    void recreateSwapchain();
    void recreateSurface();
    bool handlePresentOrAcquireResult(Graph::SwapchainStatus status, bool imageAcquired);

    void waitForValidExtent() const;

    Graph::Window m_Window;
    TKit::TierArray<Event> m_Events;

    TKit::Timespan m_MonitorDeltaTime{};

    Graph::Swapchain m_Swapchain = Graph::NullHandle;
    // NOTE(Isma, 03/07/26): Overlays create their own views so as of right now they are bound by that limit. overlays
    // could share the same view if allocated the same window, so the limit is not very accurate
    TKit::StaticArray<Overlay *, ONYX_MAX_VIEWS> m_Overlays{};

    mutable f32v2 m_PrevMousePos{0.f};

    Graph::PresentMode m_PresentMode;
    bool m_MustRecreateSwapchain = false;
};
} // namespace Onyx
