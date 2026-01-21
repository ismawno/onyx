#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/camera.hpp"
#include "onyx/property/options.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/rendering/renderer.hpp"
#include "vkit/presentation/swap_chain.hpp"
#include "tkit/memory/block_allocator.hpp"
#include "tkit/profiling/timespan.hpp"
#include "tkit/container/static_array.hpp"

struct GLFWwindow;

namespace Onyx
{
u32 ToFrequency(TKit::Timespan p_DeltaTime);
TKit::Timespan ToDeltaTime(u32 p_FrameRate);

using WindowFlags = u8;
enum WindowFlagBit : WindowFlags
{
    WindowFlag_Resizable = 1 << 0,
    WindowFlag_Visible = 1 << 1,
    WindowFlag_Decorated = 1 << 2,
    WindowFlag_Focused = 1 << 3,
    WindowFlag_Floating = 1 << 4,
};

enum TransferMode : u8
{
    Transfer_Separate = 0,
    Transfer_SameIndex = 1,
    Transfer_SameQueue = 2
};

using Timeout = u64;

constexpr Timeout Block = TKIT_U64_MAX;
constexpr Timeout Poll = 0;

class Window
{
    TKIT_NON_COPYABLE(Window)
  public:
    static constexpr VkSurfaceFormatKHR SurfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    static constexpr VkFormat DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    struct ImageData
    {
        VKit::DeviceImage *Presentation;
        VKit::DeviceImage DepthStencil;
    };
    struct Specs
    {
        const char *Name = "Onyx window";
        u32v2 Position{TKIT_U32_MAX}; // u32 max means let it be decided automatically
        u32v2 Dimensions{800, 600};
        VkPresentModeKHR PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        WindowFlags Flags = WindowFlag_Resizable | WindowFlag_Visible | WindowFlag_Decorated | WindowFlag_Focused;
    };

    Window();
    Window(const Specs &p_Specs);
    ~Window();

    /**
     * @brief Begin rendering and recording the frame's command buffer.
     *
     * After this call command buffer dependent operations that require to be recorded in a
     * `vkBeginRendering()`/`vkEndRendering()` pair may be submitted.
     *
     */
    void BeginRendering(VkCommandBuffer p_CommandBuffer, const Color &p_ClearColor = Color::BLACK);
    void EndRendering(VkCommandBuffer p_CommandBuffer);

    bool ShouldClose() const;

    const GLFWwindow *GetWindowHandle() const
    {
        return m_Window;
    }
    GLFWwindow *GetWindowHandle()
    {
        return m_Window;
    }

    void EnableVSync()
    {
        SetPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }
    void DisableVSync(const VkPresentModeKHR p_PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
        SetPresentMode(p_PresentMode);
    }
    bool IsVSync() const
    {
        return m_PresentMode == VK_PRESENT_MODE_FIFO_KHR || m_PresentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    const char *GetName() const
    {
        return m_Name;
    }

    const u32v2 &GetPosition() const
    {
        return m_Position;
    }

    const u32v2 &GetScreenDimensions() const
    {
        return m_Dimensions;
    }
    u32 GetScreenWidth() const
    {
        return m_Dimensions[0];
    }
    u32 GetScreenHeight() const
    {
        return m_Dimensions[1];
    }

    u32 GetPixelWidth() const
    {
        return m_SwapChain.GetInfo().Extent.width;
    }
    u32 GetPixelHeight() const
    {
        return m_SwapChain.GetInfo().Extent.height;
    }
    u32v2 GetPixelDimensions() const
    {
        return u32v2{GetPixelWidth(), GetPixelHeight()};
    }

    f32 GetScreenAspect() const
    {
        return static_cast<f32>(m_Dimensions[0]) / static_cast<f32>(m_Dimensions[1]);
    }
    f32 GetPixelAspect() const
    {
        return static_cast<f32>(GetPixelWidth()) / static_cast<f32>(GetPixelHeight());
    }

    WindowFlags GetFlags() const
    {
        return m_Flags;
    }

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
     * @param p_Default If no monitor is found, a user provided default.
     * @return The delta time of the monitor.
     */
    TKit::Timespan UpdateMonitorDeltaTime(TKit::Timespan p_Default = TKit::Timespan::FromSeconds(1.f / 60.f));

    TKit::Timespan GetMonitorDeltaTime() const
    {
        return m_MonitorDeltaTime;
    }

    void FlagShouldClose();
    void PushEvent(const Event &p_Event);

    const TKit::StaticArray<Event, MaxEvents> &GetNewEvents() const
    {
        return m_Events;
    }

    void FlushEvents();

    template <Dimension D> Camera<D> *CreateCamera()
    {
        auto &array = getCameraArray<D>();
        Camera<D> *camera = m_Allocator.Create<Camera<D>>();
        camera->m_Window = this;
        camera->adaptViewToViewportAspect();

        array.Append(camera);
        return camera;
    }

    template <Dimension D> Camera<D> *CreateCamera(const CameraOptions &p_Options)
    {
        Camera<D> *camera = CreateCamera<D>();
        camera->SetViewport(p_Options.Viewport);
        camera->SetScissor(p_Options.Scissor);
        return camera;
    }

    template <Dimension D> Camera<D> *GetCamera(const u32 p_Index = 0)
    {
        auto &array = getCameraArray<D>();
        return array[p_Index];
    }

    template <Dimension D> void DestroyCamera(const u32 p_Index = 0)
    {
        auto &array = getCameraArray<D>();
        m_Allocator.Destroy(array[p_Index]);
        array.RemoveOrdered(array.begin() + p_Index);
    }

    template <Dimension D> void DestroyCamera(const Camera<D> *p_Camera)
    {
        auto &array = getCameraArray<D>();
        for (u32 i = 0; i < array.GetSize(); ++i)
            if (array[i] == p_Camera)
            {
                DestroyCamera<D>(i);
                return;
            }
    }

    template <Dimension D> TKit::StaticArray<Detail::CameraInfo<D>, MaxCameras> GetCameraInfos() const
    {
        auto &array = getCameraArray<D>();
        TKit::StaticArray<Detail::CameraInfo<D>, MaxCameras> cameras;
        for (const Camera<D> *cam : array)
            cameras.Append(cam->CreateCameraInfo());
        return cameras;
    }

    bool AcquireNextImage(Timeout p_Timeout = Block);
    void Present(const Renderer::RenderSubmitInfo &p_Info);

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

    void SetPresentMode(const VkPresentModeKHR p_PresentMode)
    {
        if (m_PresentMode == p_PresentMode)
            return;
        m_MustRecreateSwapchain = true;
        m_PresentMode = p_PresentMode;
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

  private:
    void recreateSurface();
    void createWindow(const Specs &p_Specs);
    void adaptCamerasToViewportAspect();
    void createSwapChain(const VkExtent2D &p_WindowExtent);
    void recreateSwapChain();
    void recreateResources();
    bool handleImageResult(VkResult p_Result);

    TKit::TierArray<ImageData> createImageData();
    void destroyImageData();

    VkExtent2D waitGlfwEvents();
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

    TKit::StaticArray<Camera<D2> *, MaxCameras> m_Cameras2{};
    TKit::StaticArray<Camera<D3> *, MaxCameras> m_Cameras3{};

    TKit::BlockAllocator m_Allocator;

    TKit::StaticArray<Event, MaxEvents> m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};

    TKit::TierArray<ImageData> m_Images{};
    VKit::SwapChain m_SwapChain;

    TKit::TierArray<Execution::SyncData> m_SyncData{};

    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_ImageAvailableIndex = 0;
    TransferMode m_TransferMode;
    u64 m_ViewBit;

    const char *m_Name;
    u32v2 m_Position;
    u32v2 m_Dimensions;
#ifdef TKIT_ENABLE_INSTRUMENTATION
    u32 m_ColorIndex = 0;
#endif

    struct
    {
        VkSemaphore Timeline = VK_NULL_HANDLE;
        u64 InFlightValue = 0;
    } m_LastGraphicsSubmission{};

    VkPresentModeKHR m_PresentMode;
    WindowFlags m_Flags;
    bool m_MustRecreateSwapchain = false;

    friend void windowResizeCallback(GLFWwindow *, const i32, const i32);
};
} // namespace Onyx
