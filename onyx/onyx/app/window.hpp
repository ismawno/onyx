#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/execution/sync.hpp"
#include "vkit/presentation/swap_chain.hpp"

#include "tkit/memory/block_allocator.hpp"

struct GLFWwindow;

namespace Onyx
{
template <Dimension D> using RenderContextArray = TKit::Array<RenderContext<D> *, MaxRenderContexts>;
template <Dimension D> using CameraArray = TKit::Array<Camera<D> *, MaxCameras>;

using EventArray = TKit::Array<Event, MaxEvents>;

u32 ToFrequency(TKit::Timespan p_DeltaTime);
TKit::Timespan ToDeltaTime(u32 p_FrameRate);

struct FrameInfo
{
    VkCommandBuffer GraphicsCommand = VK_NULL_HANDLE;
    VkCommandBuffer TransferCommand = VK_NULL_HANDLE;
    u32 FrameIndex;

    operator bool() const
    {
        return GraphicsCommand != VK_NULL_HANDLE;
    }
};

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

struct WaitMode
{
    u64 WaitFenceTimeout;
    u64 AcquireTimeout;

    static const WaitMode Block;
    static const WaitMode Poll;
};

class Window
{
    TKIT_NON_COPYABLE(Window)
  public:
    struct ImageData
    {
        VKit::DeviceImage *Presentation;
        VKit::DeviceImage DepthStencil;
    };
    struct CommandData
    {
        VKit::CommandPool GraphicsPool;
        VKit::CommandPool TransferPool;
        VkCommandBuffer GraphicsCommand;
        VkCommandBuffer TransferCommand;
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

    FrameInfo BeginFrame(const WaitMode &p_WaitMode = WaitMode::Block);

    /**
     * @brief Submit all rendering context data to the device.
     *
     * After this method, calls to render contexts are not allowed until the beginning of the next frame. At best, they
     * will have no effect.
     *
     * @return Stage flags governing transfer synchronization.
     */
    VkPipelineStageFlags SubmitContextData(const FrameInfo &p_Info);

    /**
     * @brief Begin rendering and recording the frame's command buffer.
     *
     * After this call command buffer dependent operations that require to be recorded in a
     * `vkBeginRendering()`/`vkEndRendering()` pair may be submitted.
     *
     */
    void BeginRendering(const Color &p_ClearColor = Color::BLACK);

    /**
     * @brief Record all window commands, should be called between `BeginRendering()` and `EndRendering()`.
     *
     */
    void Render(const FrameInfo &p_Info);

    void EndRendering();
    void EndFrame(VkPipelineStageFlags p_Flags);

    /**
     * @brief Executes the whole window rendering pipeline.
     *
     * Useful for simple rendering use-cases (no fancy usage of command buffers, just context-rendering) but non ideal.
     * Rendering not always can go through, and previous operations (such as context rendering recording) may go to
     * waste if this happens.
     *
     * @return `true` if the window managed to render, `false` otherwise.
     */
    bool Render(const Color &p_ClearColor = Color::BLACK);

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

    const EventArray &GetNewEvents() const
    {
        return m_Events;
    }

    void FlushEvents();

    template <Dimension D> RenderContext<D> *CreateRenderContext()
    {
        RenderContext<D> *context = m_Allocator.Create<RenderContext<D>>(CreateSceneRenderInfo());
        auto &array = getContextArray<D>();
        array.Append(context);
        return context;
    }
    template <Dimension D> RenderContext<D> *GetRenderContext(const u32 p_Index = 0)
    {
        auto &array = getContextArray<D>();
        return array[p_Index];
    }
    template <Dimension D> void DestroyRenderContext(const u32 p_Index = 0)
    {
        auto &array = getContextArray<D>();
        m_Allocator.Destroy(array[p_Index]);
        array.RemoveOrdered(array.begin() + p_Index);
    }
    template <Dimension D> void DestroyRenderContext(const RenderContext<D> *p_Context)
    {
        auto &array = getContextArray<D>();
        for (u32 i = 0; i < array.GetSize(); ++i)
            if (array[i] == p_Context)
            {
                DestroyRenderContext<D>(i);
                return;
            }
    }

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

    u32 GetFrameIndex() const
    {
        return m_FrameIndex;
    }

    VkPipelineRenderingCreateInfoKHR CreateSceneRenderInfo() const;

    VkResult AcquireNextImage(const WaitMode &p_WaitMode);

    void SubmitGraphicsQueue(VkPipelineStageFlags p_Flags);
    void SubmitTransferQueue();
    VkResult Present();

    VkCommandBuffer GetGraphicsCommandBuffer() const
    {
        return m_CommandData[m_FrameIndex].GraphicsCommand;
    }
    VkCommandBuffer GetTransferCommandBuffer() const
    {
        return m_CommandData[m_FrameIndex].TransferCommand;
    }

    VKit::Queue *GetGraphicsQueue() const
    {
        return m_Graphics;
    }

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
    const TKit::Array8<VkPresentModeKHR> &GetAvailablePresentModes() const
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

  private:
    void recreateSurface();
    void createWindow(const Specs &p_Specs);
    void adaptCamerasToViewportAspect();
    void createSwapChain(const VkExtent2D &p_WindowExtent);
    void recreateSwapChain();
    void recreateResources();
    void createCommandData();

    bool handleImageResult(VkResult p_Result);

    PerImageData<ImageData> createImageData();
    void destroyImageData();

    VkExtent2D waitGlfwEvents();

    template <Dimension D> auto &getContextArray()
    {
        if constexpr (D == D2)
            return m_RenderContexts2;
        else
            return m_RenderContexts3;
    }
    template <Dimension D> auto &getCameraArray()
    {
        if constexpr (D == D2)
            return m_Cameras2;
        else
            return m_Cameras3;
    }

    template <Dimension D> TKit::Array<Detail::CameraInfo, MaxCameras> getCameraInfos()
    {
        auto &array = getCameraArray<D>();
        TKit::Array<Detail::CameraInfo, MaxCameras> cameras;
        for (const Camera<D> *cam : array)
            cameras.Append(cam->CreateCameraInfo());
        return cameras;
    }

    GLFWwindow *m_Window;

    RenderContextArray<D2> m_RenderContexts2;
    RenderContextArray<D3> m_RenderContexts3;

    CameraArray<D2> m_Cameras2{};
    CameraArray<D3> m_Cameras3{};

    TKit::BlockAllocator m_Allocator;

    EventArray m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};

    PerImageData<ImageData> m_Images{};

    VKit::SwapChain m_SwapChain;

    PerFrameData<CommandData> m_CommandData;

    PerFrameData<Detail::SyncFrameData> m_SyncFrameData{};
    PerImageData<Detail::SyncImageData> m_SyncImageData{};

    VKit::Queue *m_Graphics;
    VKit::Queue *m_Transfer;
    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    TransferMode m_TransferMode;

    const char *m_Name;
    u32v2 m_Position;
    u32v2 m_Dimensions;
#ifdef TKIT_ENABLE_INSTRUMENTATION
    u32 m_ColorIndex = 0;
#endif

    VkPresentModeKHR m_PresentMode;
    WindowFlags m_Flags;
    bool m_MustRecreateSwapchain = false;

    friend void windowResizeCallback(GLFWwindow *, const i32, const i32);
};
} // namespace Onyx
