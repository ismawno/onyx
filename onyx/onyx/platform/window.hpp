#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/platform/input.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/camera.hpp"
#include "onyx/property/options.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/rendering/renderer.hpp"
#include "vkit/presentation/swap_chain.hpp"
#include "tkit/profiling/timespan.hpp"

struct GLFWwindow;

namespace Onyx
{
u32 ToFrequency(TKit::Timespan deltaTime);
TKit::Timespan ToDeltaTime(u32 frameRate);

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
    void BeginRendering(VkCommandBuffer commandBuffer, const Color &clearColor = Color::Black);
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

    template <Dimension D> Camera<D> *CreateCamera()
    {
        auto &array = getCameraArray<D>();
        TKit::TierAllocator *alloc = TKit::GetTier();
        Camera<D> *camera = alloc->Create<Camera<D>>();
        camera->m_Window = this;
        camera->adaptViewToViewportAspect();

        array.Append(camera);
        return camera;
    }

    template <Dimension D> Camera<D> *CreateCamera(const CameraOptions &options)
    {
        Camera<D> *camera = CreateCamera<D>();
        camera->SetViewport(options.Viewport);
        camera->SetScissor(options.Scissor);
        return camera;
    }

    template <Dimension D> Camera<D> *GetCamera(const u32 index = 0)
    {
        auto &array = getCameraArray<D>();
        return array[index];
    }

    template <Dimension D> void DestroyCamera(const u32 index = 0)
    {
        auto &array = getCameraArray<D>();
        TKit::TierAllocator *alloc = TKit::GetTier();
        alloc->Destroy(array[index]);
        array.RemoveOrdered(array.begin() + index);
    }

    template <Dimension D> void DestroyCamera(const Camera<D> *camera)
    {
        auto &array = getCameraArray<D>();
        for (u32 i = 0; i < array.GetSize(); ++i)
            if (array[i] == camera)
            {
                DestroyCamera<D>(i);
                return;
            }
    }

    template <Dimension D> TKit::TierArray<Detail::CameraInfo<D>> GetCameraInfos() const
    {
        auto &array = getCameraArray<D>();
        TKit::TierArray<Detail::CameraInfo<D>> cameras;
        for (const Camera<D> *cam : array)
            cameras.Append(cam->CreateCameraInfo());
        return cameras;
    }

    ONYX_NO_DISCARD Result<bool> AcquireNextImage(Timeout timeout = Block);
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

    u64 GetViewBit() const
    {
        return m_ViewBit;
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

  private:
    void adaptCamerasToViewportAspect();

    ONYX_NO_DISCARD Result<> createSwapChain(const VkExtent2D &windowExtent);
    ONYX_NO_DISCARD Result<> recreateSwapChain();
    ONYX_NO_DISCARD Result<> recreateResources();
    ONYX_NO_DISCARD Result<> recreateSurface();
    ONYX_NO_DISCARD Result<bool> handleImageResult(VkResult result);

    ONYX_NO_DISCARD static Result<VKit::SwapChain> createSwapChain(VkPresentModeKHR presentMode, VkSurfaceKHR surface,
                                                                   const VkExtent2D &windowExtent,
                                                                   const VKit::SwapChain *old = nullptr);

    ONYX_NO_DISCARD static Result<TKit::TierArray<Window::ImageData>> createImageData(VKit::SwapChain &swapChain);
    static void destroyImageData(TKit::TierArray<Window::ImageData> &images);
    static VkExtent2D getNewExtent(GLFWwindow *window);

    template <Dimension D> const auto &getCameraArray() const
    {
        if constexpr (D == D2)
            return m_Cameras2;
        else
            return m_Cameras3;
    }

    template <Dimension D> auto &getCameraArray()
    {
        if constexpr (D == D2)
            return m_Cameras2;
        else
            return m_Cameras3;
    }

    GLFWwindow *m_Window;

    TKit::TierArray<Camera<D2> *> m_Cameras2{};
    TKit::TierArray<Camera<D3> *> m_Cameras3{};

    TKit::TierArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};

    VKit::SwapChain m_SwapChain;
    TKit::TierArray<ImageData> m_Images{};
    TKit::TierArray<Execution::SyncData> m_SyncData{};

    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_ImageAvailableIndex = 0;
    u64 m_ViewBit;

    VkPresentModeKHR m_PresentMode;
    bool m_MustRecreateSwapchain = false;

    friend Result<Window *> Platform::CreateWindow(const WindowSpecs &);
};
} // namespace Onyx
