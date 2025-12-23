#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"
#include "tkit/memory/block_allocator.hpp"

#ifndef ONYX_MAX_RENDER_CONTEXTS
#    define ONYX_MAX_RENDER_CONTEXTS 16
#endif

#ifndef ONYX_MAX_CAMERAS
#    define ONYX_MAX_CAMERAS 16
#endif

#ifndef ONYX_MAX_EVENTS
#    define ONYX_MAX_EVENTS 32
#endif

struct GLFWwindow;

namespace Onyx
{
template <Dimension D> using RenderContextArray = TKit::StaticArray<RenderContext<D> *, ONYX_MAX_RENDER_CONTEXTS>;
template <Dimension D> using CameraArray = TKit::StaticArray<Camera<D> *, ONYX_MAX_CAMERAS>;

using EventArray = TKit::StaticArray<Event, ONYX_MAX_EVENTS>;

u32 ToFrequency(TKit::Timespan p_DeltaTime);
TKit::Timespan ToDeltaTime(u32 p_FrameRate);

struct FrameInfo
{
    VkCommandBuffer GraphicsCommand;
    VkCommandBuffer TransferCommand;
    u32 FrameIndex;

    operator bool() const
    {
        return GraphicsCommand != VK_NULL_HANDLE;
    }
};

/**
 * @brief Represents a window in the Onyx application.
 *
 * This class encapsulates the functionality of a window, including rendering, input handling,
 * and window properties like size, position, and visibility.
 */
class ONYX_API Window
{
    TKIT_NON_COPYABLE(Window)
  public:
    using Flags = u8;
    /**
     * @brief Flags representing window properties.
     *
     * These flags can be used to specify window attributes such as resizable, visible, decorated, etc.
     *
     * - Flag_Resizable: The window can be resized.
     *
     * - Flag_Visible: THe window will be visible when created.
     *
     * - Flag_Decorated: The window had decorations such as border, close button etc.
     *
     * - Flag_Focused: The window is focused upon creation.
     *
     * - Flag_Floating: The window is always on top of other windows.
     */
    enum FlagBit : Flags
    {
        Flag_None = 0,
        Flag_Resizable = 1 << 0,
        Flag_Visible = 1 << 1,
        Flag_Decorated = 1 << 2,
        Flag_Focused = 1 << 3,
        Flag_Floating = 1 << 4,
    };
    /**
     * @brief Specifications for creating a window.
     *
     * Contains parameters like name, dimensions, and flags.
     */
    struct Specs
    {
        const char *Name = "Onyx window";
        u32v2 Position{TKit::Limits<u32>::Max()}; // u32 max means let it be decided automatically
        u32v2 Dimensions{800, 600};
        VkPresentModeKHR PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        Flags Flags = Flag_Resizable | Flag_Visible | Flag_Decorated | Flag_Focused;
    };

    Window();
    Window(const Specs &p_Specs);
    ~Window();

    /**
     * @brief Begin a frame for the given window.
     *
     * This method will wait for the next available swap chain image, so it may block for some time.
     * After this call, `ImGui` and `ImPlot` may be used until the `EndRendering()` method.
     *
     * @param p_WaitMode The timeouts when waiting for fences and acquiring next image.
     * @return Information about the new frame.
     */
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
    void BeginRendering();

    /**
     * @brief Record all window commands.
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
    bool Render();

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
        m_FrameScheduler->SetPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }
    void DisableVSync(const VkPresentModeKHR p_PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
        m_FrameScheduler->SetPresentMode(p_PresentMode);
    }
    bool IsVSync() const
    {
        const VkPresentModeKHR pm = m_FrameScheduler->GetPresentMode();
        return pm == VK_PRESENT_MODE_FIFO_KHR || pm == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    const FrameScheduler *GetFrameScheduler() const
    {
        return m_FrameScheduler.Get();
    }
    FrameScheduler *GetFrameScheduler()
    {
        return m_FrameScheduler.Get();
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
        return m_FrameScheduler->GetSwapChain().GetInfo().Extent.width;
    }
    u32 GetPixelHeight() const
    {
        return m_FrameScheduler->GetSwapChain().GetInfo().Extent.height;
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

    Flags GetFlags() const
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

    /**
     * @brief Gets the new events since the last call to `FlushEvents()`.
     *
     * @return The array of new events.
     */
    const EventArray &GetNewEvents() const
    {
        return m_Events;
    }

    void FlushEvents();

    template <Dimension D> RenderContext<D> *CreateRenderContext()
    {
        RenderContext<D> *context = m_Allocator.Create<RenderContext<D>>(m_FrameScheduler->CreateSceneRenderInfo());
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

    Color BackgroundColor = Color::BLACK;

  private:
    void recreateSurface();
    void createWindow(const Specs &p_Specs);
    /**
     * @brief Scale camera views to adapt to their viewport aspects.
     *
     * This method is called automatically on window resize events so that elements in the scene are not distorted.
     */
    void adaptCamerasToViewportAspect();

    template <Dimension D> auto &getContextArray()
    {
        if constexpr (D == D2)
            return m_RenderContexts2D;
        else
            return m_RenderContexts3D;
    }
    template <Dimension D> auto &getCameraArray()
    {
        if constexpr (D == D2)
            return m_Cameras2D;
        else
            return m_Cameras3D;
    }

    template <Dimension D> TKit::StaticArray<Detail::CameraInfo, ONYX_MAX_CAMERAS> getCameraInfos()
    {
        auto &array = getCameraArray<D>();
        TKit::StaticArray<Detail::CameraInfo, ONYX_MAX_CAMERAS> cameras;
        for (const Camera<D> *cam : array)
            cameras.Append(cam->CreateCameraInfo());
        return cameras;
    }

    GLFWwindow *m_Window;

    TKit::Storage<FrameScheduler> m_FrameScheduler;

    RenderContextArray<D2> m_RenderContexts2D;
    RenderContextArray<D3> m_RenderContexts3D;

    CameraArray<D2> m_Cameras2D{};
    CameraArray<D3> m_Cameras3D{};

    TKit::BlockAllocator m_Allocator;

    EventArray m_Events;
    VkSurfaceKHR m_Surface;

    TKit::Timespan m_MonitorDeltaTime{};

    const char *m_Name;
    u32v2 m_Position;
    u32v2 m_Dimensions;
#ifdef TKIT_ENABLE_INSTRUMENTATION
    u32 m_ColorIndex = 0;
#endif
    Flags m_Flags;

    friend class FrameScheduler;
    friend void windowResizeCallback(GLFWwindow *, const i32, const i32);
};
} // namespace Onyx
