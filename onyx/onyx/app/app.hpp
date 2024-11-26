#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/theme.hpp"
#include "tkit/profiling/clock.hpp"

namespace Onyx
{
/**
 * @brief This class provides a simple application interface, with some common functionality.
 *
 * This base class can represent a single or multiple window application.
 *
 */
class IApplication
{
  public:
    /**
     * @brief Construct a new IApplication object.
     *
     */
    IApplication() = default;

    /**
     * @brief Destroy the IApplication object.
     *
     */
    virtual ~IApplication() noexcept;

    /**
     * @brief Startup the application and call the OnStart() method of all layers.
     *
     * This method must be called before calling NextFrame().
     * Calling this method more than once will result in undefined behaviour or a crash.
     *
     */
    virtual void Startup() noexcept;

    /**
     * @brief Shutdown the application, clean up some resources and call the OnShutdown() method of all layers.
     *
     * This method must be called after the Startup() call, after the last call to NextFrame(), once all windows have
     * been closed. Failing to do so, or calling this method more than once will result in undefined behaviour or a
     * crash.
     *
     */
    void Shutdown() noexcept;

    /**
     * @brief This method is in charge of processing and presenting the next frame for all windows.
     *
     * This method should be called in a loop until it returns false, which means that all windows have been closed.
     * All of its calls should reside between Startup() and Shutdown().
     *
     * @param p_Clock A clock that lets both the API and the user to keep track of the frame time.
     * @return Whether the application still contains opened windows.
     */
    virtual bool NextFrame(TKit::Clock &p_Clock) noexcept = 0;

    /**
     * @brief Get the time it took the last frame to process.
     *
     * @return TKit::Timespan The delta time of the last frame.
     */
    virtual TKit::Timespan GetDeltaTime() const noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return const Window* The main window at index 0.
     */
    virtual const Window *GetMainWindow() const noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return Window* The main window at index 0.
     */
    virtual Window *GetMainWindow() noexcept = 0;

    /**
     * @brief Set an object derived from Theme to apply an ImGui theme.
     *
     * @tparam T User defined theme.
     * @tparam ThemeArgs Arguments to pass to the theme constructor.
     * @param p_Args Arguments to pass to the theme constructor.
     * @return T* Pointer to the theme object.
     */
    template <std::derived_from<Theme> T, typename... ThemeArgs> T *SetTheme(ThemeArgs &&...p_Args) noexcept
    {
        auto theme = TKit::Scope<T>::Create(std::forward<ThemeArgs>(p_Args)...);
        T *result = theme.Get();
        m_Theme = std::move(theme);
        m_Theme->Apply();
        return result;
    }

    /**
     * @brief Apply the current theme to ImGui. Use SetTheme() to set a new theme.
     *
     */
    void ApplyTheme() noexcept;

    /**
     * @brief Run the whole application in one go.
     *
     * This method automatically calls Startup(), NextFrame() and Shutdown(). If you choose to run the application this
     * way, you must not use any of the other 3 methods.
     *
     */
    void Run() noexcept;

    /**
     * @brief Check if the Startup method has been called.
     *
     * @return true if Startup() has been called.
     */
    bool IsStarted() const noexcept;

    /**
     * @brief Check if the Shutdown method has been called.
     *
     * @return true if Shutdown() has been called.
     */
    bool IsTerminated() const noexcept;

    /**
     * @brief Check if the Startup method has been called and the Shutdown method has not been called.
     *
     * @return true if the application is running.
     */
    bool IsRunning() const noexcept;

    /// The layer system managing application layers.
    LayerSystem Layers;

  protected:
    /**
     * @brief Initialize ImGui for the given window.
     *
     * @param p_Window The window to initialize ImGui for.
     */
    void initializeImGui(Window &p_Window) noexcept;

    /**
     * @brief Shutdown ImGui and release its resources.
     *
     */
    void shutdownImGui() noexcept;

    /**
     * @brief Begin rendering ImGui frame.
     *
     */
    static void beginRenderImGui() noexcept;

    /**
     * @brief End rendering ImGui frame and submit commands.
     *
     * @param p_CommandBuffer The command buffer to record commands into.
     */
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    /// Reference to the Vulkan device.
    TKit::Ref<Device> m_Device;

  private:
    /**
     * @brief Create the ImGui descriptor pool.
     *
     */
    void createImGuiPool() noexcept;

    /// Indicates whether Startup() has been called.
    bool m_Started = false;

    /// Indicates whether Shutdown() has been called.
    bool m_Terminated = false;

    /// Vulkan descriptor pool used by ImGui.
    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;

    /// Current theme applied to ImGui.
    TKit::Scope<Theme> m_Theme;
};

/**
 * @brief A standard, single window application.
 *
 * It is the simplest form of an application available, and works as one would expect.
 *
 */
class Application final : public IApplication
{
  public:
    /**
     * @brief Construct a new Application object with the given window specifications.
     *
     * @param p_WindowSpecs Specifications for the main window.
     */
    Application(const Window::Specs &p_WindowSpecs = {}) noexcept;

    /**
     * @brief Process and present the next frame for the application.
     *
     * @param p_Clock A clock to keep track of frame time.
     * @return true if the application should continue running.
     */
    bool NextFrame(TKit::Clock &p_Clock) noexcept override;

    /**
     * @brief Get the main window.
     *
     * @return const Window* Pointer to the main window.
     */
    const Window *GetMainWindow() const noexcept override;

    /**
     * @brief Get the main window.
     *
     * @return Window* Pointer to the main window.
     */
    Window *GetMainWindow() noexcept override;

    /**
     * @brief Get the time it took the last frame to process.
     *
     * @return TKit::Timespan The delta time of the last frame.
     */
    TKit::Timespan GetDeltaTime() const noexcept override;

  private:
    /// Storage for the main window.
    TKit::Storage<Window> m_Window;

    /// The time elapsed between frames.
    TKit::Timespan m_DeltaTime;
};

} // namespace Onyx
