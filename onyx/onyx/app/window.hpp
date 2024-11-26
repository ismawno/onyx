#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Onyx
{
/**
 * @brief Represents a window in the ONYX application.
 *
 * This class encapsulates the functionality of a window, including rendering, input handling,
 * and window properties like size, position, and visibility.
 */
class ONYX_API Window
{
    TKIT_NON_COPYABLE(Window)
  public:
    /**
     * @brief Flags representing window properties.
     *
     * These flags can be used to specify window attributes such as resizable, visible, decorated, etc.
     */
    enum Flags : u8
    {
        RESIZABLE = 1 << 0, ///< The window can be resized.
        VISIBLE = 1 << 1,   ///< The window is visible upon creation.
        DECORATED = 1 << 2, ///< The window has decorations such as a border, close button, etc.
        FOCUSED = 1 << 3,   ///< The window is focused upon creation.
        FLOATING = 1 << 4   ///< The window is always on top of other windows.
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
        u8 Flags = RESIZABLE | VISIBLE | DECORATED | FOCUSED;
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
     * This method begins a new frame, starts the render pass with the specified background color,
     * executes the provided draw and UI callables, and ends the render pass and frame.
     *
     * @tparam F1 Type of the draw calls callable.
     * @tparam F2 Type of the UI calls callable.
     * @param p_DrawCalls Callable for custom draw calls.
     * @param p_UICalls Callable for custom UI calls.
     * @return true if rendering was successful, false otherwise.
     */
    template <typename F1, typename F2> bool Render(F1 &&p_DrawCalls, F2 &&p_UICalls) noexcept
    {
        TKIT_PROFILE_NSCOPE("Onyx::Window::Render");
        if (const VkCommandBuffer cmd = m_FrameScheduler->BeginFrame(*this))
        {
            {
                TKIT_PROFILE_VULKAN_SCOPE("Onyx::Window::Render", m_Device->GetProfilingContext(), cmd);
                m_FrameScheduler->BeginRenderPass(BackgroundColor);
                {
                    TKIT_PROFILE_VULKAN_NAMED_SCOPE(vkDrawCalls, "Onyx::DrawCalls", m_Device->GetProfilingContext(),
                                                    cmd, true);
                    TKIT_PROFILE_NAMED_NSCOPE(drawCalls, "Onyx::DrawCalls", true);
                    std::forward<F1>(p_DrawCalls)(cmd);
                }

                // This bit is profiled inside the renderer methods.
                m_RenderContext2D->Render(cmd);
                m_RenderContext3D->Render(cmd);

                {
                    TKIT_PROFILE_VULKAN_NAMED_SCOPE(vkUiCalls, "Onyx::ImGui", m_Device->GetProfilingContext(), cmd,
                                                    true);
                    TKIT_PROFILE_NAMED_NSCOPE(uiCalls, "Onyx::ImGui", true);
                    std::forward<F2>(p_UICalls)(cmd);
                }
                m_FrameScheduler->EndRenderPass();
            }

            {
#ifdef TKIT_ENABLE_VULKAN_PROFILING
                static TKIT_PROFILE_DECLARE_MUTEX(std::mutex, mutex);
                std::scoped_lock lock(mutex);
                TKIT_PROFILE_MARK_LOCK(mutex);
#endif
                TKIT_PROFILE_VULKAN_COLLECT(m_Device->GetProfilingContext(), cmd);
            }
            m_FrameScheduler->EndFrame(*this);
            return true;
        }
        return false;
    }
    /**
     * @brief Renders the window without any custom draw or UI calls.
     *
     * @return true if rendering was successful, false otherwise.
     */
    bool Render() noexcept;
    /**
     * @brief Checks if the window should close.
     *
     * @return true if the window should close, false otherwise.
     */
    bool ShouldClose() const noexcept;

    /**
     * @brief Gets the GLFW window handle.
     *
     * @return const GLFWwindow* Pointer to the GLFW window.
     */
    const GLFWwindow *GetWindowHandle() const noexcept;
    /**
     * @brief Gets the GLFW window handle.
     *
     * @return GLFWwindow* Pointer to the GLFW window.
     */
    GLFWwindow *GetWindowHandle() noexcept;

    /**
     * @brief Gets the window's name.
     *
     * @return const char* The window's name.
     */
    const char *GetName() const noexcept;
    /**
     * @brief Gets the window's width in screen coordinates.
     *
     * @return u32 The window's width.
     */
    u32 GetScreenWidth() const noexcept;
    /**
     * @brief Gets the window's height in screen coordinates.
     *
     * @return u32 The window's height.
     */
    u32 GetScreenHeight() const noexcept;

    /**
     * @brief Gets the window's width in pixels.
     *
     * @return u32 The window's width in pixels.
     */
    u32 GetPixelWidth() const noexcept;
    /**
     * @brief Gets the window's height in pixels.
     *
     * @return u32 The window's height in pixels.
     */
    u32 GetPixelHeight() const noexcept;

    /**
     * @brief Gets the aspect ratio of the window in screen coordinates.
     *
     * @return f32 The screen aspect ratio.
     */
    f32 GetScreenAspect() const noexcept;
    /**
     * @brief Gets the aspect ratio of the window in pixels.
     *
     * @return f32 The pixel aspect ratio.
     */
    f32 GetPixelAspect() const noexcept;

    /**
     * @brief Gets the position of the window on the screen.
     *
     * @return std::pair<u32, u32> The (x, y) position of the window.
     */
    std::pair<u32, u32> GetPosition() const noexcept;

    /**
     * @brief Checks if the window was resized.
     *
     * @return true if the window was resized since the last call to FlagResizeDone().
     */
    bool WasResized() const noexcept;
    /**
     * @brief Flags the window as resized with the new dimensions.
     *
     * @param p_Width New width of the window.
     * @param p_Height New height of the window.
     */
    void FlagResize(u32 p_Width, u32 p_Height) noexcept;
    /**
     * @brief Resets the resized flag after handling a resize event.
     */
    void FlagResizeDone() noexcept;
    /**
     * @brief Flags the window to be closed.
     */
    void FlagShouldClose() noexcept;

    /**
     * @brief Gets the Vulkan surface associated with the window.
     *
     * @return VkSurfaceKHR The Vulkan surface.
     */
    VkSurfaceKHR GetSurface() const noexcept;

    /**
     * @brief Pushes a new event to the window's event queue.
     *
     * @param p_Event The event to push.
     */
    void PushEvent(const Event &p_Event) noexcept;
    /**
     * @brief Gets the new events since the last call to FlushEvents().
     *
     * @return const DynamicArray<Event>& The array of new events.
     */
    const DynamicArray<Event> &GetNewEvents() const noexcept;
    /**
     * @brief Clears the window's event queue.
     */
    void FlushEvents() noexcept;

    /**
     * @brief Gets the render context for the specified dimension.
     *
     * @tparam D The dimension (D2 or D3).
     * @return const RenderContext<D>* Pointer to the render context.
     */
    template <Dimension D> const RenderContext<D> *GetRenderContext() const noexcept
    {
        if constexpr (D == D2)
            return m_RenderContext2D.Get();
        else
            return m_RenderContext3D.Get();
    }
    /**
     * @brief Gets the render context for the specified dimension.
     *
     * @tparam D The dimension (D2 or D3).
     * @return RenderContext<D>* Pointer to the render context.
     */
    template <Dimension D> RenderContext<D> *GetRenderContext() noexcept
    {
        if constexpr (D == D2)
            return m_RenderContext2D.Get();
        else
            return m_RenderContext3D.Get();
    }

    /**
     * @brief Gets the frame scheduler.
     *
     * @return const FrameScheduler& Reference to the frame scheduler.
     */
    const FrameScheduler &GetFrameScheduler() const noexcept;
    /**
     * @brief Gets the frame scheduler.
     *
     * @return FrameScheduler& Reference to the frame scheduler.
     */
    FrameScheduler &GetFrameScheduler() noexcept;

    /// The background color used when clearing the window.
    Color BackgroundColor = Color::BLACK;

  private:
    /**
     * @brief Creates the window with the given specifications.
     *
     * @param p_Specs The specifications for the window.
     */
    void createWindow(const Specs &p_Specs) noexcept;

    GLFWwindow *m_Window;

    TKit::Ref<Instance> m_Instance;
    TKit::Ref<Device> m_Device;

    TKit::Storage<FrameScheduler> m_FrameScheduler;
    TKit::Storage<RenderContext<D2>> m_RenderContext2D;
    TKit::Storage<RenderContext<D3>> m_RenderContext3D;

    DynamicArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;

    bool m_Resized = false;
};
} // namespace Onyx
