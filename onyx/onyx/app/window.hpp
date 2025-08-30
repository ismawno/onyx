#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"
#include "tkit/memory/block_allocator.hpp"
#include "onyx/core/glfw.hpp"

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

    /**
     * @brief Constructs a window with default specifications.
     */
    Window() noexcept;
    /**
     * @brief Constructs a window with the given specifications.
     *
     * @param p_Specs Specifications for the window.
     */
    explicit Window(const Specs &p_Specs) noexcept;
    /**
     * @brief Destructor.
     *
     * Cleans up resources associated with the window.
     */
    ~Window() noexcept;

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
    bool Render(const RenderCallbacks &p_Callbacks = {}) noexcept;

    bool ShouldClose() const noexcept;

    const GLFWwindow *GetWindowHandle() const noexcept;
    GLFWwindow *GetWindowHandle() noexcept;

    const FrameScheduler *GetFrameScheduler() const noexcept;
    FrameScheduler *GetFrameScheduler() noexcept;

    const char *GetName() const noexcept;

    u32 GetScreenWidth() const noexcept;
    u32 GetScreenHeight() const noexcept;

    u32 GetPixelWidth() const noexcept;
    u32 GetPixelHeight() const noexcept;

    f32 GetScreenAspect() const noexcept;
    f32 GetPixelAspect() const noexcept;

    Flags GetFlags() const noexcept;

    /**
     * @brief Gets the position of the window on the screen.
     *
     * @return The (x, y) position of the window.
     */
    std::pair<u32, u32> GetPosition() const noexcept;

    void FlagShouldClose() noexcept;

    VkSurfaceKHR GetSurface() const noexcept;

    void PushEvent(const Event &p_Event) noexcept;

    /**
     * @brief Gets the new events since the last call to `FlushEvents()`.
     *
     * @return The array of new events.
     */
    const EventArray &GetNewEvents() const noexcept;

    void FlushEvents() noexcept;

    template <Dimension D> RenderContext<D> *CreateRenderContext() noexcept
    {
        RenderContext<D> *context = m_Allocator.Create<RenderContext<D>>(m_FrameScheduler->CreateSceneRenderInfo());
        auto &array = getContextArray<D>();
        array.Append(context);
        return context;
    }
    template <Dimension D> RenderContext<D> *GetRenderContext(const u32 p_Index = 0) noexcept
    {
        auto &array = getContextArray<D>();
        return array[p_Index];
    }
    template <Dimension D> void DestroyRenderContext(const u32 p_Index = 0) noexcept
    {
        auto &array = getContextArray<D>();
        m_Allocator.Destroy(array[p_Index]);
        array.RemoveOrdered(array.begin() + p_Index);
    }
    template <Dimension D> void DestroyRenderContext(const RenderContext<D> *p_Context) noexcept
    {
        auto &array = getContextArray<D>();
        for (u32 i = 0; i < array.GetSize(); ++i)
            if (array[i] == p_Context)
            {
                DestroyRenderContext<D>(i);
                return;
            }
    }

    template <Dimension D> Camera<D> *CreateCamera() noexcept
    {
        auto &array = getCameraArray<D>();
        Camera<D> *camera = m_Allocator.Create<Camera<D>>();
        camera->m_Window = this;
        camera->adaptViewToViewportAspect();

        array.Append(camera);
        return camera;
    }

    template <Dimension D> Camera<D> *CreateCamera(const CameraOptions &p_Options) noexcept
    {
        Camera<D> *camera = CreateCamera<D>();
        camera->SetViewport(p_Options.Viewport);
        camera->SetScissor(p_Options.Scissor);
        return camera;
    }

    template <Dimension D> Camera<D> *GetCamera(const u32 p_Index = 0) noexcept
    {
        auto &array = getCameraArray<D>();
        return array[p_Index];
    }

    template <Dimension D> void DestroyCamera(const u32 p_Index = 0) noexcept
    {
        auto &array = getCameraArray<D>();
        m_Allocator.Destroy(array[p_Index]);
        array.RemoveOrdered(array.begin() + p_Index);
    }

    template <Dimension D> void DestroyCamera(const Camera<D> *p_Camera) noexcept
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
    void flagResize(u32 p_Width, u32 p_Height) noexcept;

  private:
    bool wasResized() const noexcept;
    void flagResizeDone() noexcept;
    void recreateSurface() noexcept;

    void createWindow(const Specs &p_Specs) noexcept;
    /**
     * @brief Scale camera views to adapt to their viewport aspects.
     *
     * This method is called automatically on window resize events so that elements in the scene are not distorted.
     */
    void adaptCamerasToViewportAspect() noexcept;

    template <Dimension D> auto &getContextArray() noexcept
    {
        if constexpr (D == D2)
            return m_RenderContexts2D;
        else
            return m_RenderContexts3D;
    }
    template <Dimension D> auto &getCameraArray() noexcept
    {
        if constexpr (D == D2)
            return m_Cameras2D;
        else
            return m_Cameras3D;
    }

    template <Dimension D> TKit::StaticArray<Detail::CameraInfo, ONYX_MAX_CAMERAS> getCameraInfos() noexcept
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
