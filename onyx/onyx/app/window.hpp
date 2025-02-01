#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"

#include "onyx/core/glfw.hpp"

namespace Onyx
{
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
    /**
     * @brief Flags representing window properties.
     *
     * These flags can be used to specify window attributes such as resizable, visible, decorated, etc.
     */
    enum FlagBits : u8
    {
        Flag_Resizable = 1 << 0, ///< The window can be resized.
        Flag_Visible = 1 << 1,   ///< The window is visible upon creation.
        Flag_Decorated = 1 << 2, ///< The window has decorations such as a border, close button, etc.
        Flag_Focused = 1 << 3,   ///< The window is focused upon creation.
        Flag_Floating = 1 << 4   ///< The window is always on top of other windows.
    };
    using Flags = u8;
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
     * This method begins a new frame, starts the render pass with the specified background color,
     * executes the provided draw and UI callables, and ends the render pass and frame.
     *
     * @tparam F1 Type of the draw calls callable.
     * @tparam F2 Type of the UI calls callable.
     * @param p_FirstDraws Callable for draw calls that happen before the main scene rendering.
     * @param p_LastDraws Callable for draw calls that happen after the main scene rendering.
     * @return true if rendering was successful, false otherwise.
     */
    template <typename F1, typename F2> bool Render(F1 &&p_FirstDraws, F2 &&p_LastDraws) noexcept
    {
        TKIT_PROFILE_NSCOPE("Onyx::Window::Render");
        const VkCommandBuffer cmd = m_FrameScheduler->BeginFrame(*this);
        if (!cmd)
            return false;

        {
            TKIT_PROFILE_VULKAN_SCOPE("Onyx::Window::Window::Render", Core::GetProfilingContext(), cmd);
            m_FrameScheduler->BeginRenderPass(BackgroundColor);

            std::forward<F1>(p_FirstDraws)(cmd);

            m_RenderContext2D->Render(cmd);
            m_RenderContext3D->Render(cmd);

            std::forward<F2>(p_LastDraws)(cmd);

            m_FrameScheduler->EndRenderPass();
        }

        {
#ifdef TKIT_ENABLE_VULKAN_PROFILING
            static TKIT_PROFILE_DECLARE_MUTEX(std::mutex, mutex);
            std::scoped_lock lock(mutex);
            TKIT_PROFILE_MARK_LOCK(mutex);
#endif
            TKIT_PROFILE_VULKAN_COLLECT(Core::GetProfilingContext(), cmd);
        }
        m_FrameScheduler->EndFrame(*this);
        return true;
    }

    /**
     * @brief Renders the window using the provided draw callables.
     *
     * This method begins a new frame, starts the render pass with the specified background color,
     * executes the provided draw callables, and ends the render pass and frame.
     *
     * @tparam F Type of the draw calls callable.
     * @param p_FirstDraws Callable for draw calls that happen before the main scene rendering.
     * @return true if rendering was successful, false otherwise.
     */
    template <typename F> bool RenderSubmitFirst(F &&p_FirstDraws) noexcept
    {
        return Render(std::forward<F>(p_FirstDraws), [](const VkCommandBuffer) {});
    }

    /**
     * @brief Renders the window using the provided UI callables.
     *
     * This method begins a new frame, starts the render pass with the specified background color,
     * executes the provided UI callables, and ends the render pass and frame.
     *
     * @tparam F Type of the UI calls callable.
     * @param p_LastDraws Callable for draw calls that happen after the main scene rendering.
     * @return true if rendering was successful, false otherwise.
     */
    template <typename F> bool RenderSubmitLast(F &&p_LastDraws) noexcept
    {
        return Render([](const VkCommandBuffer) {}, std::forward<F>(p_LastDraws));
    }

    /**
     * @brief Renders the window without any custom draw or UI calls.
     *
     * This method begins a new frame, starts the render pass with the specified background color,
     * executes the provided draw callables, and ends the render pass and frame.
     *
     * @return true if rendering was successful, false otherwise.
     */
    bool Render() noexcept;

    bool ShouldClose() const noexcept;

    const GLFWwindow *GetWindowHandle() const noexcept;
    GLFWwindow *GetWindowHandle() noexcept;

    const char *GetName() const noexcept;

    u32 GetScreenWidth() const noexcept;
    u32 GetScreenHeight() const noexcept;

    u32 GetPixelWidth() const noexcept;
    u32 GetPixelHeight() const noexcept;

    f32 GetScreenAspect() const noexcept;
    f32 GetPixelAspect() const noexcept;

    /**
     * @brief Gets the position of the window on the screen.
     *
     * @return The (x, y) position of the window.
     */
    std::pair<u32, u32> GetPosition() const noexcept;

    /**
     * @brief Sets up the post-processing pipeline, which is used to apply effects to the scene after the main rendering
     * pass.
     *
     * If you wish to switch to a different post-processing pipeline, call this method again with the new
     * specifications. Do not call RemovePostProcessing before or after that in the same frame, as that call will
     * override the setup.
     *
     * @param p_Layout The pipeline layout to use for the post-processing pipeline.
     * @param p_FragmentShader The fragment shader to use for the post-processing pipeline.
     * @param p_VertexShader Optional vertex shader to use for the post-processing pipeline.
     * @param p_Info Optional sampler information to use for the post-processing pipeline.
     * @return A pointer to the post-processing pipeline.
     */
    PostProcessing *SetPostProcessing(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
                                      const VKit::Shader *p_VertexShader = nullptr,
                                      const VkSamplerCreateInfo *p_Info = nullptr) noexcept;

    PostProcessing *GetPostProcessing() noexcept;

    /**
     * @brief Removes the post-processing pipeline and substitutes it with a naive one that simply blits the final
     * image.
     */
    void RemovePostProcessing() noexcept;

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

    /**
     * @brief Gets the render context for the specified dimension.
     *
     * @tparam D The dimension (`D2` or `D3`).
     * @return Pointer to the render context.
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
     * @tparam D The dimension (`D2` or `D3`).
     * @return Pointer to the render context.
     */
    template <Dimension D> RenderContext<D> *GetRenderContext() noexcept
    {
        if constexpr (D == D2)
            return m_RenderContext2D.Get();
        else
            return m_RenderContext3D.Get();
    }

    /**
     * @brief Waits for the `FrameScheduler` to finish submitting the current frame.
     *
     * Blocks the calling thread until all command buffers have been submitted.
     */
    void WaitForFrameSubmission() noexcept;

    const VKit::RenderPass &GetRenderPass() const noexcept;
    u32 GetFrameIndex() const noexcept;

    VkPresentModeKHR GetPresentMode() const noexcept;
    const TKit::StaticArray8<VkPresentModeKHR> &GetAvailablePresentModes() const noexcept;

    void SetPresentMode(VkPresentModeKHR p_PresentMode) noexcept;

    /// The background color used when clearing the window.
    Color BackgroundColor = Color::BLACK;

    bool wasResized() const noexcept;
    void flagResize(u32 p_Width, u32 p_Height) noexcept;
    void flagResizeDone() noexcept;

  private:
    void createWindow(const Specs &p_Specs) noexcept;

    GLFWwindow *m_Window;

    TKit::Storage<Detail::FrameScheduler> m_FrameScheduler;
    TKit::Storage<RenderContext<D2>> m_RenderContext2D;
    TKit::Storage<RenderContext<D3>> m_RenderContext3D;

    TKit::StaticArray32<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;

    bool m_Resized = false;
};
} // namespace Onyx
