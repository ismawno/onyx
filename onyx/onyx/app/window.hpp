#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "tkit/container/storage.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/profiling/vulkan.hpp"

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
    using Flags = u8;
    /**
     * @brief Flags representing window properties.
     *
     * These flags can be used to specify window attributes such as resizable, visible, decorated, etc.
     */
    enum FlagBit : Flags
    {
        Flag_Resizable = 1 << 0, ///< The window can be resized.
        Flag_Visible = 1 << 1,   ///< The window is visible upon creation.
        Flag_Decorated = 1 << 2, ///< The window has decorations such as a border, close button, etc.
        Flag_Focused = 1 << 3,   ///< The window is focused upon creation.
        Flag_Floating = 1 << 4   ///< The window is always on top of other windows.
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
     * This method begins a new frame, starts the rendering with the specified background color,
     * executes the provided draw and UI callables, and ends the rendering and frame.
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
            m_FrameScheduler->BeginRendering(BackgroundColor);

            const u32 frameIndex = m_FrameScheduler->GetFrameIndex();
            std::forward<F1>(p_FirstDraws)(frameIndex, cmd);

            for (const auto &context : m_RenderContexts2D)
                context->GrowToFit(frameIndex);
            for (const auto &context : m_RenderContexts3D)
                context->GrowToFit(frameIndex);

            TKit::ITaskManager *tm = Core::GetTaskManager();

            TKit::Task<> *render = tm->CreateAndSubmit([this, frameIndex, cmd]() {
                for (const auto &context : m_RenderContexts2D)
                    context->Render(frameIndex, cmd);
                for (const auto &context : m_RenderContexts3D)
                    context->Render(frameIndex, cmd);
            });

            for (const auto &context : m_RenderContexts2D)
                context->SendToDevice(frameIndex);
            for (const auto &context : m_RenderContexts3D)
                context->SendToDevice(frameIndex);

            tm->WaitUntilFinished(render);
            tm->DestroyTask(render);

            std::forward<F2>(p_LastDraws)(frameIndex, cmd);

            m_FrameScheduler->EndRendering();
        }

        {
#ifdef TKIT_ENABLE_VULKAN_PROFILING
            static TKIT_PROFILE_DECLARE_MUTEX(std::mutex, mutex);
            TKIT_PROFILE_MARK_LOCK(mutex);
#endif
            TKIT_PROFILE_VULKAN_COLLECT(Core::GetProfilingContext(), cmd);
        }
        m_FrameScheduler->EndFrame();
        return true;
    }

    /**
     * @brief Renders the window using the provided draw callables.
     *
     * This method begins a new frame, starts the rendering with the specified background color,
     * executes the provided draw callables, and ends the rendering and frame.
     *
     * @tparam F Type of the draw calls callable.
     * @param p_FirstDraws Callable for draw calls that happen before the main scene rendering.
     * @return true if rendering was successful, false otherwise.
     */
    template <typename F> bool RenderSubmitFirst(F &&p_FirstDraws) noexcept
    {
        return Render(std::forward<F>(p_FirstDraws), [](const u32, const VkCommandBuffer) {});
    }

    /**
     * @brief Renders the window using the provided UI callables.
     *
     * This method begins a new frame, starts the rendering with the specified background color,
     * executes the provided UI callables, and ends the rendering and frame.
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
     * This method begins a new frame, starts the rendering with the specified background color,
     * executes the provided draw callables, and ends the rendering and frame.
     *
     * @return true if rendering was successful, false otherwise.
     */
    bool Render() noexcept;

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

    bool m_Resized = false;
};
} // namespace Onyx
