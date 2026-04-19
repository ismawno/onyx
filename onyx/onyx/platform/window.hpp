#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/platform/input.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/property/color.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/rendering/renderer.hpp"
#include "vkit/presentation/swap_chain.hpp"
#include "tkit/profiling/clock.hpp"

struct GLFWwindow;

namespace Onyx
{
u32 ToFrequency(TKit::Timespan deltaTime);
TKit::Timespan ToDeltaTime(u32 frameRate);

template <Dimension D> struct Camera;

using Timeout = u64;

constexpr Timeout Block = TKIT_U64_MAX;
constexpr Timeout Poll = 0;

class Window
{
    TKIT_NON_COPYABLE(Window)
    struct ImageData
    {
        VKit::DeviceImage *Presentation;
        VKit::DeviceImage DepthStencil;
    };

  public:
    static constexpr VkSurfaceFormatKHR SurfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    static constexpr VkFormat DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    Window() = default;
    ~Window();

    static Window *FromHandle(GLFWwindow *window);

    /**
     * @brief Begin rendering and recording the frame's command buffer.
     *
     * After this call command buffer dependent operations that require to be recorded in a
     * `vkBeginRendering()`/`vkEndRendering()` pair may be submitted.
     *
     */
    void BeginRendering(VkCommandBuffer commandBuffer);
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
        SetPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }
    void DisableVSync(const VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
        SetPresentMode(presentMode);
    }
    bool IsVSync() const
    {
        return m_PresentMode == VK_PRESENT_MODE_FIFO_KHR || m_PresentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
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
    RenderView<D> *CreateRenderView(Camera<D> *camera, const ScreenViewport &viewport = {},
                                    const ScreenScissor &scissor = {})
    {
        TKit::TierAllocator *tier = TKit::GetTier();
        RenderView<D> *rv = tier->Create<RenderView<D>>(camera, viewport, scissor);
        rv->updateExtent(m_SwapChain.GetInfo().Extent);
        return getRenderViews<D>().Append(rv);
    }

    template <Dimension D> void DestroyRenderView(const RenderView<D> *rv)
    {
        TKit::TierArray<RenderView<D> *> &rvs = getRenderViews<D>();
        for (u32 i = 0; i < rvs.GetSize(); ++i)
            if (rv == rvs[i])
            {
                TKit::TierAllocator *tier = TKit::GetTier();
                tier->Destroy(rvs[i]);
                rvs.RemoveOrdered(rvs.begin() + i);
                return;
            }
        TKIT_FATAL("[ONYX][WINDOW] Render view '{}' not found", scast<const void *>(rv));
    }

    template <Dimension D> RenderView<D> *GetMouseRenderView() const
    {
        const TKit::TierArray<RenderView<D> *> &rvs = GetRenderViews<D>();
        const f32v2 mpos = GetScreenMousePosition();

        for (u32 i = rvs.GetSize() - 1; i < rvs.GetSize(); --i)
            if (rvs[i]->IsWithinViewport(mpos))
                return rvs[i];

        return nullptr;
    }

    template <Dimension D> TKit::TierArray<ViewInfo<D>> CreateViewInfos() const
    {
        const TKit::TierArray<RenderView<D> *> &views = GetRenderViews<D>();
        TKit::TierArray<ViewInfo<D>> infos{};
        for (RenderView<D> *rv : views)
        {
            rv->CacheMatrices();
            infos.Append(rv->CreateViewInfo());
        }
        return infos;
    }

    RenderTargetInfo CreateRenderTargetInfo()
    {
        RenderTargetInfo info;
        info.Views2 = CreateViewInfos<D2>();
        info.Views3 = CreateViewInfos<D3>();
        info.ImageAvailableSemaphore = GetImageAvailableSemaphore();
        info.RenderFinishedSemaphore = GetRenderFinishedSemaphore();
        return info;
    }

    template <Dimension D>
    void ControlCamera(TKit::Timespan deltaTime, Camera<D> *camera, const CameraControls<D> &controls = {}) const;

    ONYX_NO_DISCARD Result<bool> AcquireNextImage(Timeout timeout);
    ONYX_NO_DISCARD Result<> Present();

    void RequestSwapchainRecreation()
    {
        m_MustRecreateSwapchain = true;
    }

    const VKit::SwapChain &GetSwapChain() const
    {
        return m_SwapChain;
    }

    VkPresentModeKHR GetPresentMode() const
    {
        return m_PresentMode;
    }
    const TKit::TierArray<VkPresentModeKHR> &GetAvailablePresentModes() const
    {
        return m_SwapChain.GetInfo().SupportDetails.PresentModes;
    }

    void SetPresentMode(const VkPresentModeKHR presentMode)
    {
        if (m_PresentMode == presentMode)
            return;
        m_MustRecreateSwapchain = true;
        m_PresentMode = presentMode;
    }

    VkSemaphore GetImageAvailableSemaphore() const
    {
        return m_SyncData[m_ImageAvailableIndex].ImageAvailableSemaphore;
    }
    VkSemaphore GetRenderFinishedSemaphore() const
    {
        return m_SyncData[m_ImageIndex].RenderFinishedSemaphore;
    }
    void MarkSubmission(const VkSemaphore timeline, const u64 inFlightValue)
    {
        m_SyncData[m_ImageAvailableIndex].InFlightSubmission = timeline;
        m_SyncData[m_ImageAvailableIndex].InFlightValue = inFlightValue;
    }

    template <Dimension D> const TKit::TierArray<RenderView<D> *> &GetRenderViews() const
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

    Color ClearColor = Color::Black;

  private:
    void updateRenderViews();

    ONYX_NO_DISCARD Result<> createSwapChain(const VkExtent2D &windowExtent);
    ONYX_NO_DISCARD Result<> drainWork();
    ONYX_NO_DISCARD Result<> recreateSwapChain();
    ONYX_NO_DISCARD Result<> recreateResources();
    ONYX_NO_DISCARD Result<> recreateSurface();
    ONYX_NO_DISCARD Result<bool> handlePresentOrAcquireResult(VkResult result);
    ONYX_NO_DISCARD Result<> nameSurface();
    ONYX_NO_DISCARD Result<> nameSwapChain();
    ONYX_NO_DISCARD Result<> nameSyncData();
    ONYX_NO_DISCARD Result<> nameImageData();

    ONYX_NO_DISCARD static Result<VKit::SwapChain> createSwapChain(VkPresentModeKHR presentMode, VkSurfaceKHR surface,
                                                                   const VkExtent2D &windowExtent,
                                                                   const VKit::SwapChain *old = nullptr);

    ONYX_NO_DISCARD static Result<TKit::TierArray<Window::ImageData>> createImageData(VKit::SwapChain &swapChain);
    static void destroyImageData(TKit::TierArray<Window::ImageData> &images);
    static VkExtent2D getNewExtent(GLFWwindow *window);

    template <Dimension D> TKit::TierArray<RenderView<D> *> &getRenderViews()
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

    GLFWwindow *m_Window;

    TKit::TierArray<RenderView<D2> *> m_RenderViews2{};
    TKit::TierArray<RenderView<D3> *> m_RenderViews3{};

    TKit::TierArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};
    TKit::Clock m_TimeSinceResize{};

    VKit::SwapChain m_SwapChain;
    TKit::TierArray<ImageData> m_Images{};
    TKit::TierArray<Execution::ViewSyncData> m_SyncData{};

    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_ImageAvailableIndex = 0;
    mutable f32v2 m_PrevMousePos{0.f};

    VkPresentModeKHR m_PresentMode;
    bool m_MustRecreateSwapchain = false;

    friend Result<Window *> Platform::CreateWindow(const WindowSpecs &);
};
} // namespace Onyx
