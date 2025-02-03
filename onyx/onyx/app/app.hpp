#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/theme.hpp"
#include "tkit/profiling/clock.hpp"

namespace Onyx
{
/**
 * @brief This class provides a simple application interface, with some common functionality.
 *
 * This base class can represent a single or multi-window application.
 *
 */
class IApplication
{
  public:
    IApplication() = default;

    virtual ~IApplication() noexcept;

    /**
     * @brief Startup the application and call the `OnStart()` method of all layers.
     *
     * This method must be called before calling `NextFrame()`.
     * Calling this method more than once will result in undefined behaviour or a crash.
     *
     */
    virtual void Startup() noexcept;

    /**
     * @brief Shutdown the application, clean up some resources and call the `OnShutdown()` method of all layers.
     *
     * This method must be called after the `Startup()` call, after the last call to `NextFrame()`, once all windows
     * have been closed. Failing to do so, or calling this method more than once will result in undefined behaviour or a
     * crash.
     *
     */
    void Shutdown() noexcept;

    TKit::Timespan GetDeltaTime() const noexcept;

    /**
     * @brief This method is in charge of processing and presenting the next frame for all windows.
     *
     * This method should be called in a loop until it returns false, which means that all windows have been closed.
     * All of its calls should reside between `Startup()` and `Shutdown()`.
     *
     * @param p_Clock A clock that lets both the API and the user to keep track of the frame time.
     * @return Whether the application still contains opened windows.
     */
    virtual bool NextFrame(TKit::Clock &p_Clock) noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0 in multi-window applications.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return The main window at index 0.
     */
    virtual const Window *GetMainWindow() const noexcept = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * In concurrent mode, that window is always handled by the main thread.
     *
     * @return The main window at index 0.
     */
    virtual Window *GetMainWindow() noexcept = 0;

    /**
     * @brief Set an object derived from Theme to apply an `ImGui` theme.
     *
     * @tparam T User defined theme.
     * @tparam ThemeArgs Arguments to pass to the theme constructor.
     * @param p_Args Arguments to pass to the theme constructor.
     * @return Pointer to the theme object.
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
     * @brief Set a new user layer to provide custom functionality.
     *
     * This method will delete the current user layer and replace it with a new one. If called in the middle of a frame,
     * the operation will be deferred until the end of the frame.
     *
     * @tparam T User defined layer.
     * @tparam LayerArgs Arguments to pass to the layer constructor.
     * @param p_Args Arguments to pass to the layer constructor.
     * @return Pointer to the new user layer.
     */
    template <std::derived_from<UserLayer> T, typename... LayerArgs> T *SetUserLayer(LayerArgs &&...p_Args) noexcept
    {
        T *layer = new T(std::forward<LayerArgs>(p_Args)...);
        if (m_DeferFlag)
        {
            delete m_StagedUserLayer;
            m_StagedUserLayer = layer;
        }
        else
        {
            delete m_UserLayer;
            m_UserLayer = layer;
        }
        return layer;
    }

    template <std::derived_from<UserLayer> T = UserLayer> T *GetUserLayer() noexcept
    {
        return static_cast<T *>(m_UserLayer);
    }

    /**
     * @brief Apply the current theme to `ImGui`. Use `SetTheme()` to set a new theme.
     *
     */
    void ApplyTheme() noexcept;

    /**
     * @brief Run the whole application in one go.
     *
     * This method automatically calls `Startup()`, `NextFrame()` and `Shutdown()`. If you choose to run the application
     * this way, you must not use any of the other 3 methods.
     *
     */
    void Run() noexcept;

    /**
     * @brief Check if the 'Startup()` method has been called.
     *
     * @return true if `Startup()` has been called.
     */
    bool IsStarted() const noexcept;

    /**
     * @brief Check if the `Shutdown()` method has been called.
     *
     * @return true if `Shutdown()` has been called.
     */
    bool IsTerminated() const noexcept;

    /**
     * @brief Check if the 'Startup()` method has been called and the `Shutdown()` method has not been called.
     *
     * @return true if the application is running.
     */
    bool IsRunning() const noexcept;

  protected:
    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    void updateUserLayerPointer() noexcept;

    void onStart() noexcept;
    void onShutdown() noexcept;

    void onUpdate() noexcept;
    void onRender(VkCommandBuffer p_CommandBuffer) noexcept;
    void onLateRender(VkCommandBuffer p_CommandBuffer) noexcept;
    void onEvent(const Event &p_Event) noexcept;

    void onUpdate(u32 p_WindowIndex) noexcept;
    void onRender(u32 p_WindowIndex, VkCommandBuffer p_CommandBuffer) noexcept;
    void onLateRender(u32 p_WindowIndex, VkCommandBuffer p_CommandBuffer) noexcept;
    void onEvent(u32 p_WindowIndex, const Event &p_Event) noexcept;
    void onImGuiRender() noexcept;

    TKit::Timespan m_DeltaTime;
    bool m_DeferFlag = false;

  private:
    void createImGuiPool() noexcept;

    UserLayer *m_UserLayer = nullptr;
    UserLayer *m_StagedUserLayer = nullptr;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
    TKit::Scope<Theme> m_Theme;

    bool m_Started = false;
    bool m_Terminated = false;
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
    Application(const Window::Specs &p_WindowSpecs = {}) noexcept;

    /**
     * @brief Process and present the next frame for the application.
     *
     * @param p_Clock A clock to keep track of frame time.
     * @return true if the application should continue running.
     */
    bool NextFrame(TKit::Clock &p_Clock) noexcept override;

    const Window *GetMainWindow() const noexcept override;
    Window *GetMainWindow() noexcept override;

  private:
    TKit::Storage<Window> m_Window;
};

/**
 * @brief A multi-window application.
 *
 * This class provides an implementation for a multi-window application. Because of the ability of having multiple
 * windows, the user must always explicitly open windows from the application's API, including the main (first) window
 * before entering the rendering loop.
 *
 * To better manage window lifetimes, calls to `OpenWindow()` or `CloseWindow()` may be deferred if called from within
 * an ongoing frame. Never update your state based on the calls of these functions, but rather react to the
 * corresponding events (`WindowOpened`, `WindowClosed`) to ensure synchronization between the API and the user.
 *
 */
class MultiWindowApplication final : public IApplication
{
    TKIT_NON_COPYABLE(MultiWindowApplication)
  public:
    MultiWindowApplication() = default;

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. Only react to
     * the window addition through the corresponding event (WindowOpened) unless you are sure that the window is being
     * added outside the frame loop.
     *
     * @param p_Specs
     */
    void OpenWindow(const Window::Specs &p_Specs = {}) noexcept;

    /**
     * @brief Close the window at the given index.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     * @param p_Index The index of the window to close.
     */
    void CloseWindow(u32 p_Index) noexcept;

    /**
     * @brief Close the given window.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     * @param p_Window The window to close.
     */
    void CloseWindow(const Window *p_Window) noexcept;

    /**
     * @brief Close all windows.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     */
    void CloseAllWindows() noexcept;

    /**
     * @brief Get a pointer to the window at the specified index.
     *
     * @param p_Index The index of the window to retrieve.
     * @return Pointer to the window at the given index.
     */
    const Window *GetWindow(u32 p_Index) const noexcept;

    /**
     * @brief Get a pointer to the window at the specified index.
     *
     * @param p_Index The index of the window to retrieve.
     * @return Pointer to the window at the given index.
     */
    Window *GetWindow(u32 p_Index) noexcept;

    /**
     * @brief Get a pointer to the main window (at `index = 0`).
     *
     * @return Pointer to the main window.
     */
    const Window *GetMainWindow() const noexcept override;

    /**
     * @brief Get a pointer to the main window (at `index = 0`).
     *
     * @return Pointer to the main window.
     */
    Window *GetMainWindow() noexcept override;

    u32 GetWindowCount() const noexcept;

    /**
     * @brief Proceed to the next frame of the application.
     *
     * @param p_Clock The clock used to measure frame time.
     * @return true if the application should continue running, false otherwise.
     */
    bool NextFrame(TKit::Clock &p_Clock) noexcept override;

  private:
    template <typename F1, typename F2>
    void processFrame(u32 p_WindowIndex, F1 &&p_FirstDrawCalls, F2 &&p_LastDrawCalls) noexcept;
    void processWindows() noexcept;

    TKit::StaticArray8<TKit::Scope<Window>> m_Windows;
    TKit::StaticArray4<Window::Specs> m_WindowsToAdd;
};

} // namespace Onyx
