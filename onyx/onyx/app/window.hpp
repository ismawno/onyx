#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"
#include "tkit/memory/block_allocator.hpp"

#include <functional>

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

struct RenderCallbacks
{
    std::function<void(u32, VkCommandBuffer)> OnFrameBegin = nullptr;
    std::function<void(u32, VkCommandBuffer)> OnRenderBegin = nullptr;
    std::function<void(u32, VkCommandBuffer)> OnRenderEnd = nullptr;
    std::function<void(u32, VkCommandBuffer)> OnFrameEnd = nullptr;
    std::function<void(u32)> OnBadFrame = nullptr;
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
        u32 Width = 800;
        u32 Height = 600;
        VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        Flags Flags = Flag_Resizable | Flag_Visible | Flag_Decorated | Flag_Focused;
    };

    Window();
    Window(const Specs &p_Specs);
    ~Window();

    /**
     * @brief Renders the window using the provided draw and UI callables.
     *
     * This method begins a new frame, starts rendering with the vulkan API,
     * executes the provided callbacks, and ends the rendering and frame.
     *
     * Vulkan command recording outside the render callbacks may cause undefined behaviour.
     *
     * @param p_Callbacks Render callbacks to customize rendering behaviour and submit custom render commands within the
     * frame loop.
     *
     * @return true if rendering was successful, false otherwise.
     */
    bool Render(const RenderCallbacks &p_Callbacks = {});

    bool ShouldClose() const;

    const GLFWwindow *GetWindowHandle() const
    {
        return m_Window;
    }
    GLFWwindow *GetWindowHandle()
    {
        return m_Window;
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

    u32 GetScreenWidth() const
    {
        return m_Width;
    }
    u32 GetScreenHeight() const
    {
        return m_Height;
    }

    u32 GetPixelWidth() const
    {
        return m_FrameScheduler->GetSwapChain().GetInfo().Extent.width;
    }
    u32 GetPixelHeight() const
    {
        return m_FrameScheduler->GetSwapChain().GetInfo().Extent.height;
    }

    f32 GetScreenAspect() const
    {
        return static_cast<f32>(m_Width) / static_cast<f32>(m_Height);
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
     * @brief Gets the position of the window on the screen.
     *
     * @return The (x, y) position of the window.
     */
    std::pair<u32, u32> GetPosition() const;

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

    /// The background color used when clearing the window.
    Color BackgroundColor = Color::BLACK;

    // Implementation detail but needs to be public
    void flagResize(u32 p_Width, u32 p_Height);

  private:
    bool wasResized() const;
    void flagResizeDone();
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

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;
    Flags m_Flags;

    bool m_Resized = false;

    friend class FrameScheduler;
};
} // namespace Onyx
