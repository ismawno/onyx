#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/theme.hpp"
#include "kit/profiling/clock.hpp"

namespace ONYX
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
    IApplication() = default;
    virtual ~IApplication() noexcept;

    /**
     * @brief Startup the application and calls the OnStart() method of all layers.
     *
     * This method must be called before calling NextFrame().
     * Calling this method more than once will result in undefined behaviour or a crash.
     *
     */
    virtual void Startup() noexcept;

    /**
     * @brief Shutdown the application, cleans up some resources and calls the OnShutdown() method of all layers.
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
    virtual bool NextFrame(KIT::Clock &p_Clock) noexcept = 0;

    /**
     * @brief Get the time it took the last frame to process.
     *
     */
    virtual KIT::Timespan GetDeltaTime() const noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return Window at index 0
     */
    virtual const Window *GetMainWindow() const noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return Window at index 0
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
        auto theme = KIT::Scope<T>::Create(std::forward<ThemeArgs>(p_Args)...);
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
     */
    bool IsStarted() const noexcept;

    /**
     * @brief Check if the Shutdown method has been called.
     *
     */
    bool IsTerminated() const noexcept;

    /**
     * @brief Check if the Startup method has been called and the Shutdown method has not been called.
     *
     */
    bool IsRunning() const noexcept;

    LayerSystem Layers;

  protected:
    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    // Up to the user to get the device once a window is created
    KIT::Ref<Device> m_Device;

  private:
    void createImGuiPool() noexcept;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
    KIT::Scope<Theme> m_Theme;
};

class Application final : public IApplication
{
  public:
    Application(const Window::Specs &p_WindowSpecs = {}) noexcept;

    bool NextFrame(KIT::Clock &p_Clock) noexcept override;

    const Window *GetMainWindow() const noexcept override;
    Window *GetMainWindow() noexcept override;

    KIT::Timespan GetDeltaTime() const noexcept override;

  private:
    KIT::Storage<Window> m_Window;
    KIT::Timespan m_DeltaTime;
};

} // namespace ONYX