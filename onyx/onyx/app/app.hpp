#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/theme.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/memory/ptr.hpp"

#ifdef ONYX_ENABLE_IMGUI
struct ImGuiContext;
#endif
#ifdef ONYX_ENABLE_IMPLOT
struct ImPlotContext;
#endif

#ifndef ONYX_MAX_WINDOWS
#    define ONYX_MAX_WINDOWS 8
#endif

namespace Onyx
{
using WindowArray = TKit::StaticArray<Window *, ONYX_MAX_WINDOWS>;
/**
 * @brief This class provides a simple application interface, with some common functionality.
 *
 * This base class can represent a single or multi-window application. It is strongly discouraged (although technically
 * possible) to use multiple applications in the same program. If you do so, note that the following (among other
 * possible unknown things) may work incorrectly:
 *
 * - ImGui: Currently, the application manages an ImGui context, and although that context is set at
 * the beginning of each frame to ensure consistency, ImGui itself discourages using multiple.
 *
 * - Draw call profile plot: The draw call count is global for the whole program, but it would be registered and reset
 * by each application in turn, yielding weird results.
 *
 * - `UserLayer::DisplayFrameTime()`: This method uses static variables. Using many applications would result in
 * inconsistencies.
 *
 * The most common reason for a user to want to use multiple applications is to have different windows. In that case,
 * please use the `MultiWindowApplication` class, which is designed to handle multiple windows.
 *
 */
class ONYX_API IApplication
{
  public:
#ifdef ONYX_ENABLE_IMGUI
    IApplication(i32 p_ImGuiConfigFlags);
#else
    IApplication() = default;
#endif

    virtual ~IApplication();

    /**
     * @brief Startup the application and call the `OnStart()` method of all layers.
     *
     * This method must be called before calling `NextFrame()`.
     * Calling this method more than once will result in undefined behaviour or a crash.
     *
     */
    void Startup();

    /**
     * @brief Shutdown the application, clean up some resources and call the `OnShutdown()` method of all layers.
     *
     * This method must be called after the `Startup()` call, after the last call to `NextFrame()`, once all windows
     * have been closed. Failing to do so, or calling this method more than once will result in undefined behaviour or a
     * crash. This method will close all remaining windows.
     *
     */
    virtual void Shutdown();

    /**
     * @brief Signals the application to stop the frame loop.
     *
     * This method will only tell the applicatio to return `false` in the next call to `NextFrame()`. It will not close
     * any windows. Calling `NextFrame()` after this method is safe.
     *
     */
    void Quit();

    /**
     * @brief This method is in charge of processing and presenting the next frame for all windows.
     *
     * This method should be called in a loop until it returns false, which means that all windows have been closed.
     * All of its calls should reside between `Startup()` and `Shutdown()`.
     *
     * @param p_Clock A clock that lets both the API and the user to keep track of the frame time.
     * @return Whether the application still contains opened windows.
     */
    virtual bool NextFrame(TKit::Clock &p_Clock) = 0;

    /**
     * @brief Get the main window, which is always the window at index 0 in multi-window applications.
     *
     * @return The main window at index 0.
     */
    virtual const Window *GetMainWindow() const = 0;

    /**
     * @brief Get the main window, which is always the window at index 0.
     *
     * @return The main window at index 0.
     */
    virtual Window *GetMainWindow() = 0;

    /**
     * @brief Set an object derived from Theme to apply an `ImGui` theme.
     *
     * @tparam T User defined theme.
     * @tparam ThemeArgs Arguments to pass to the theme constructor.
     * @param p_Args Arguments to pass to the theme constructor.
     * @return Pointer to the theme object.
     */
    template <std::derived_from<Theme> T, typename... ThemeArgs> T *SetTheme(ThemeArgs &&...p_Args)
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
    template <std::derived_from<UserLayer> T, typename... LayerArgs> T *SetUserLayer(LayerArgs &&...p_Args)
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

    template <std::derived_from<UserLayer> T = UserLayer> T *GetUserLayer()
    {
        return static_cast<T *>(m_UserLayer);
    }

    /**
     * @brief Apply the current theme to `ImGui`. Use `SetTheme()` to set a new theme.
     *
     */
    void ApplyTheme();

    /**
     * @brief Run the whole application in one go.
     *
     * This method automatically calls `Startup()`, `NextFrame()` and `Shutdown()`. If you choose to run the application
     * this way, you must not use any of the other 3 methods.
     *
     */
    void Run();

    bool IsStarted() const
    {
        return m_Started;
    }
    bool IsTerminated() const
    {
        return m_Terminated;
    }
    bool IsRunning() const
    {
        return m_Started && !m_Terminated;
    }
    TKit::Timespan GetDeltaTime() const
    {
        return m_DeltaTime;
    }

  protected:
#ifdef ONYX_ENABLE_IMGUI
    void initializeImGui(Window &p_Window);
    void shutdownImGui();
    void setImContexts();

    static void beginRenderImGui();
    void endRenderImGui(VkCommandBuffer p_CommandBuffer);
#endif

    void updateUserLayerPointer();

    void onStart();
    void onShutdown();

    void onUpdate();
    void onFrameBegin(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onFrameEnd(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onRenderBegin(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onRenderEnd(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onEvent(const Event &p_Event);

    void onUpdate(u32 p_WindowIndex);
    void onFrameBegin(u32 p_WindowIndex, u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onFrameEnd(u32 p_WindowIndex, u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onRenderBegin(u32 p_WindowIndex, u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onRenderEnd(u32 p_WindowIndex, u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer);
    void onEvent(u32 p_WindowIndex, const Event &p_Event);
#ifdef ONYX_ENABLE_IMGUI
    void onImGuiRender();
#endif

    TKit::Timespan m_DeltaTime;
    bool m_DeferFlag = false;
    bool m_QuitFlag = false; // Contemplate adding onQuit?

  private:
#ifdef ONYX_ENABLE_IMGUI
    void createImGuiPool();
#endif

    UserLayer *m_UserLayer = nullptr;
    UserLayer *m_StagedUserLayer = nullptr;

#ifdef ONYX_ENABLE_IMGUI
    ImGuiContext *m_ImGuiContext = nullptr;
#endif
#ifdef ONYX_ENABLE_IMPLOT
    ImPlotContext *m_ImPlotContext = nullptr;
#endif

#ifdef ONYX_ENABLE_IMGUI
    i32 m_ImGuiConfigFlags = 0;
#endif
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
class ONYX_API Application final : public IApplication
{
  public:
#ifdef ONYX_ENABLE_IMGUI
    Application(const Window::Specs &p_WindowSpecs = {}, i32 p_ImGuiConfigFlags = 0);
    Application(i32 p_ImGuiConfigFlags);
#else
    Application(const Window::Specs &p_WindowSpecs = {});
#endif

    void Shutdown() override;

    /**
     * @brief Process and present the next frame for the application.
     *
     * @param p_Clock A clock to keep track of frame time.
     * @return true if the application should continue running.
     */
    bool NextFrame(TKit::Clock &p_Clock) override;

    const Window *GetMainWindow() const override
    {
        return m_Window.Get();
    }
    Window *GetMainWindow() override
    {
        return m_Window.Get();
    }

  private:
    TKit::Storage<Window> m_Window;
    bool m_WindowAlive = true;
};

/**
 * @brief A multi-window application.
 *
 * This class provides an implementation for a multi-window application. Because of the ability of having multiple
 * windows, the user must always explicitly open windows from the application's API, including the main (first) window
 * before entering the frame loop. Otherwise, the application will end immediately.
 *
 * To better manage window lifetimes, calls to `OpenWindow()` or `CloseWindow()` may be deferred if called from within
 * an ongoing frame. Never update your state based on the calls of these functions, but rather react to the
 * corresponding events (`WindowOpened`, `WindowClosed`) to ensure synchronization between the API and the user.
 *
 */
class ONYX_API MultiWindowApplication final : public IApplication
{
    TKIT_NON_COPYABLE(MultiWindowApplication)
  public:
#ifdef ONYX_ENABLE_IMGUI
    MultiWindowApplication(i32 p_ImGuiConfigFlags = 0);
#else
    MultiWindowApplication() = default;
#endif

    void Shutdown() override;

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. Only react to
     * the window addition through the corresponding event (WindowOpened) unless you are sure that the window is being
     * added outside the frame loop.
     *
     * @param p_Specs
     */
    void OpenWindow(const Window::Specs &p_Specs = {});

    /**
     * @brief Close the window at the given index.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     * @param p_Index The index of the window to close.
     */
    void CloseWindow(u32 p_Index);

    /**
     * @brief Close the given window.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     * @param p_Window The window to close.
     */
    void CloseWindow(const Window *p_Window);

    /**
     * @brief Close all windows.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     */
    void CloseAllWindows();

    const Window *GetWindow(const u32 p_Index) const
    {
        return m_Windows[p_Index];
    }
    Window *GetWindow(const u32 p_Index)
    {
        return m_Windows[p_Index];
    }

    /**
     * @brief Get a pointer to the main window (at `index = 0`).
     *
     * @return Pointer to the main window.
     */
    const Window *GetMainWindow() const override
    {
        return GetWindow(0);
    }

    /**
     * @brief Get a pointer to the main window (at `index = 0`).
     *
     * @return Pointer to the main window.
     */
    Window *GetMainWindow() override
    {
        return GetWindow(0);
    }

    u32 GetWindowCount() const
    {
        return m_Windows.GetSize();
    }

    /**
     * @brief Proceed to the next frame of the application.
     *
     * @param p_Clock The clock used to measure frame time.
     * @return true if the application should continue running, false otherwise.
     */
    bool NextFrame(TKit::Clock &p_Clock) override;

  private:
    void processFrame(u32 p_WindowIndex, const RenderCallbacks &p_Callbacks);
    void processWindows();

    WindowArray m_Windows;
    TKit::StaticArray4<Window::Specs> m_WindowsToAdd;
    TKit::BlockAllocator m_WindowAllocator = TKit::BlockAllocator::CreateFromType<Window>(ONYX_MAX_WINDOWS);
};

} // namespace Onyx
