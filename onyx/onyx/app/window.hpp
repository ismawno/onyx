#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"
#include "onyx/core/glfw.hpp"

#include <functional>

namespace Onyx
{

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
     *
     * - Flag_ConcurrentQueueSubmission: Graphics queue submissions for this window will run in a parallel thread
     * between the end of a frame and the start of the next one. Beware: this functionality is buggy when using multiple
     * windows. The surface may become lost and the application crashes. This does not seem to happen when using a
     * single window. If you are not GPU bounded or are not doing anything usefule between frames (i.e. in `OnUpdate()`
     * callbacks), dont bother with this setting.
     */
    enum FlagBit : Flags
    {
        Flag_Resizable = 1 << 0,
        Flag_Visible = 1 << 1,
        Flag_Decorated = 1 << 2,
        Flag_Focused = 1 << 3,
        Flag_Floating = 1 << 4,
        Flag_ConcurrentQueueSubmission = 1 << 5
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
    const TKit::StaticArray32<Event> &GetNewEvents() const noexcept;

    void FlushEvents() noexcept;

    template <Dimension D> RenderContext<D> *CreateRenderContext() noexcept
    {
        auto context = TKit::Scope<RenderContext<D>>::Create(this, m_FrameScheduler->CreateSceneRenderInfo());
        auto &array = getContextArray<D>();
        RenderContext<D> *ptr = context.Get();
        array.Append(std::move(context));
        return ptr;
    }
    template <Dimension D> RenderContext<D> *GetRenderContext(const u32 p_Index = 0) noexcept
    {
        auto &array = getContextArray<D>();
        return array[p_Index].Get();
    }
    template <Dimension D> void DestroyRenderContext(const u32 p_Index = 0) noexcept
    {
        auto &array = getContextArray<D>();
        array.RemoveOrdered(array.begin() + p_Index);
    }
    template <Dimension D> void DestroyRenderContext(const RenderContext<D> *p_Context) noexcept
    {
        auto &array = getContextArray<D>();
        for (u32 i = 0; i < array.GetSize(); ++i)
            if (array[i].Get() == p_Context)
            {
                DestroyRenderContext<D>(i);
                return;
            }
    }

    /// The background color used when clearing the window.
    Color BackgroundColor = Color::BLACK;

    bool wasResized() const noexcept;
    void flagResize(u32 p_Width, u32 p_Height) noexcept;
    void flagResizeDone() noexcept;
    void recreateSurface() noexcept;

  private:
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

    GLFWwindow *m_Window;

    TKit::Storage<FrameScheduler> m_FrameScheduler;
    TKit::StaticArray16<TKit::Scope<RenderContext<D2>>> m_RenderContexts2D;
    TKit::StaticArray16<TKit::Scope<RenderContext<D3>>> m_RenderContexts3D;

    TKit::StaticArray32<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;
    Flags m_Flags;

    bool m_Resized = false;
};
} // namespace Onyx
