#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/theme.hpp"
#endif
#include "tkit/profiling/clock.hpp"

#ifdef ONYX_ENABLE_IMGUI
struct ImGuiContext;
#    ifdef ONYX_ENABLE_IMPLOT
struct ImPlotContext;
#    endif
#endif

namespace Onyx
{
struct DeltaTime
{
    TKit::Timespan Target{};
    TKit::Timespan Measured{};
};

struct DeltaInfo
{
    DeltaTime Time{};
    TKit::Timespan Max{};

    // only relevant for the DisplayDeltaTime() methods. Not for logic
    TKit::Timespan Smoothed{};
    f32 Smoothness = 0.f;
    i32 Unit = 1;
    bool Limit = false;
};

using ApplicationFlags = u8;
enum ApplicationFlagBit : ApplicationFlags
{
#ifdef ONYX_ENABLE_IMGUI
    ApplicationFlag_MustEnableImGui = 1 << 0,
    ApplicationFlag_MustDisableImGui = 1 << 1,
    ApplicationFlag_ImGuiRunning = 1 << 2,
    ApplicationFlag_ImGuiEnabled = 1 << 3,
#endif
    ApplicationFlag_Defer = 1 << 4,
    ApplicationFlag_Quit = 1 << 5,
    ApplicationFlag_MustDestroyLayer = 1 << 6,
    ApplicationFlag_MustReplaceLayer = 1 << 7
};
/**
 * @brief This class provides a simple application interface.
 *
 * It can support multiple windows, with a main window that is always opened at construction. It is discouraged
 * (although technically possible) to use multiple applications in the same program. If you do so, note that the
 * following (among other possible unknown things) may work incorrectly:
 *
 * - Draw call profile plot: The draw call count is global for the whole program, but it would be registered and reset
 * by each application in turn, yielding weird results.
 *
 * The most common reason for a user to want to use multiple applications is to have different windows. In that case,
 * use a single application and open multiple windows within it.
 *
 * You may call state-altering calls to the application class anytime (such as `OpenWindow()` or `CloseWindow()`) but
 * note that such calls may be deferred if called from within an ongoing frame. Never update your state based on the
 * calls of these functions, but rather react to these events with the provided callbacks.
 *
 */
class Application
{
  public:
    struct StageClock
    {
        TKit::Clock Clock{};
        DeltaInfo Delta{};
        bool IsDue() const
        {
            return Clock.GetElapsed() >= Delta.Time.Target;
        }
        void Update()
        {
            Delta.Time.Measured = Clock.Restart();
        }
    };

    struct WindowData
    {
        Window *Window = nullptr;
        UserLayer *Layer = nullptr;
        UserLayer *StagedLayer = nullptr;
        StageClock RenderClock{};
        StageClock UpdateClock{};
#ifdef ONYX_ENABLE_IMGUI
        ImGuiContext *ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
        ImPlotContext *ImPlotContext = nullptr;
#    endif
        i32 ImGuiConfigFlags = 0;
        Theme *Theme = nullptr;
        // only relevant when displaying delta times, not for actual logic
        bool MirrorDeltas = true;
#endif
        ApplicationFlags Flags = 0;

        bool CheckFlags(const ApplicationFlags flags) const
        {
            return Flags & flags;
        }
        void SetFlags(const ApplicationFlags flags)
        {
            Flags |= flags;
        }
        void ClearFlags(const ApplicationFlags flags)
        {
            Flags &= ~flags;
        }
    };

    struct WindowSpecs
    {
        Window::Specs Specs{};
        std::function<void(Window *)> CreationCallback = nullptr;
#ifdef ONYX_ENABLE_IMGUI
        bool EnableImGui = true;
#endif
    };

    Application(const WindowSpecs &mainSpecs) : m_MainWindow(createWindow(mainSpecs))
    {
        if (mainSpecs.CreationCallback)
            mainSpecs.CreationCallback(m_MainWindow.Window);
    }
    Application(const Window::Specs &mainSpecs = {}) : Application({.Specs = mainSpecs})
    {
    }
    ~Application()
    {
        closeAllWindows();
    }

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
     *
     * @param clock A clock that lets both the API and the user to keep track of the frame time.
     * @return Whether the application must keep running.
     */
    bool NextFrame(TKit::Clock &clock);

#ifdef ONYX_ENABLE_IMGUI
    template <std::derived_from<Theme> T, typename... ThemeArgs> T *SetTheme(const Window *window, ThemeArgs &&...args)
    {
        WindowData *data = getWindowData(window);
        if (data->Theme)
            delete data->Theme;

        T *theme = new T{std::forward<ThemeArgs>(args)...};
        data->Theme = theme;
        applyTheme(*data);
        return theme;
    }
    template <std::derived_from<Theme> T, typename... ThemeArgs> T *SetTheme(ThemeArgs &&...args)
    {
        if (m_MainWindow.Theme)
            delete m_MainWindow.Theme;

        T *theme = new T{std::forward<ThemeArgs>(args)...};
        m_MainWindow.Theme = theme;
        applyTheme(m_MainWindow);
        return theme;
    }

    void ApplyTheme(const Window *window = nullptr);
#endif

    /**
     * @brief Set a new user layer to provide custom functionality.
     *
     * This method will delete the current user layer for the given window and replace it with a new one. If called in
     * the middle of a frame, the operation will be deferred until the end of the frame.
     */
    template <std::derived_from<UserLayer> T, typename... LayerArgs>
    T *SetUserLayer(Window *window, LayerArgs &&...args)
    {
        WindowData *data = getWindowData(window);
        return setUserLayer<T>(*data, std::forward<LayerArgs>(args)...);
    }
    /**
     * @brief Set a new user layer to provide custom functionality.
     *
     * This method will delete the current user layer for the main window and replace it with a new one. If called in
     * the middle of a frame, the operation will be deferred until the end of the frame.
     */
    template <std::derived_from<UserLayer> T, typename... LayerArgs> T *SetUserLayer(LayerArgs &&...args)
    {
        return setUserLayer<T>(m_MainWindow, std::forward<LayerArgs>(args)...);
    }

    /**
     * @brief Remove a user layer without destroying the resource, detaching it from the application and releasing its
     * ownership.
     *
     * This method will remove the current user layer for the given window. Ownership will be returned to the caller. If
     * called in the middle of a frame, the operation will be deferred until the end of the frame. In those cases, it is
     * UB to delete the resource until the current frame has finished.
     */
    template <std::derived_from<UserLayer> T = UserLayer> T *RemoveUserLayer(const Window *window = nullptr)
    {
        WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        UserLayer *layer = data->Layer;
        if (checkFlags(ApplicationFlag_Defer))
            data->SetFlags(ApplicationFlag_MustReplaceLayer);
        else
            data->Layer = nullptr;
        return static_cast<T>(layer);
    }

    /**
     * @brief Destroy a user layer, freeing the resource and detaching it from the application.
     *
     * This method will destroy the current user layer for the given window. If called in the middle of a frame, the
     * operation will be deferred until the end of the frame.
     */
    template <std::derived_from<UserLayer> T = UserLayer> void DestroyUserLayer(const Window *window = nullptr)
    {
        WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        if (checkFlags(ApplicationFlag_Defer))
            data->SetFlags(ApplicationFlag_MustDestroyLayer);
        else
        {
            delete data->Layer;
            data->Layer = nullptr;
            data->ClearFlags(ApplicationFlag_MustDestroyLayer);
        }
    }

    template <std::derived_from<UserLayer> T = UserLayer> const T *GetUserLayer(const Window *window = nullptr) const
    {
        const WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        return static_cast<T>(data->Layer);
    }
    template <std::derived_from<UserLayer> T = UserLayer> T *GetUserLayer(const Window *window = nullptr)
    {
        const WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        return static_cast<T>(data->Layer);
    }

    bool MustDefer() const
    {
        return checkFlags(ApplicationFlag_Defer);
    }

#ifdef __ONYX_MULTI_WINDOW
    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     */
    Window *OpenWindow(const WindowSpecs &specs)
    {
        if (checkFlags(ApplicationFlag_Defer))
        {
            m_WindowsToAdd.Append(specs);
            return nullptr;
        }
        return openWindow(specs);
    }

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     */
    Window *OpenWindow(const Window::Specs &specs = {})
    {
        return OpenWindow(WindowSpecs{.Specs = specs});
    }

    /**
     * @brief Close the given window.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the window's layer destructor, if any.
     *
     * @return Whether the operation could execute immediately.
     */
    bool CloseWindow(Window *window);
#endif

    const Window *GetMainWindow() const
    {
        return m_MainWindow.Window;
    }

    Window *GetMainWindow()
    {
        return m_MainWindow.Window;
    }

    /**
     * @brief Get the global measured delta time of the whole application's loop.
     *
     * @note This delta time does not correspond to any window but to the combined delta time of the application and can
     * give misleading values. To get the delta time of a specific window, use the other getters available.
     *
     * @return The delta time of the whole application loop.
     */
    TKit::Timespan GetDeltaTime() const
    {
        return m_DeltaTime;
    }

    const DeltaTime &GetRenderDeltaTime(const Window *window = nullptr) const
    {
        const WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        return data->RenderClock.Delta.Time;
    }
    const DeltaTime &GetUpdateDeltaTime(const Window *window = nullptr) const
    {
        const WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        return data->UpdateClock.Delta.Time;
    }

    void SetDeltaTime(const TKit::Timespan target, const Window *window = nullptr)
    {
        WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        setRenderDeltaTime(target, *data);
        setUpdateDeltaTime(target, *data);
    }
    void SetRenderDeltaTime(const TKit::Timespan target, const Window *window = nullptr)
    {
        WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        setRenderDeltaTime(target, *data);
    }
    void SetUpdateDeltaTime(const TKit::Timespan target, const Window *window = nullptr)
    {
        WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        setUpdateDeltaTime(target, *data);
    }

    /**
     * @brief Run the whole application in one go.
     *
     * This method automatically invokes an application loop, using `NextFrame()` under the hood.
     *
     */
    void Run();

#ifdef ONYX_ENABLE_IMGUI

    // these are helper methods to nicely display and edit delta times for a given window using ImGui. If used,
    // modifying the target directly inside the code by calling the delta time setter methods may interfere a bit with
    // the UI generated by these, but should not be much of a problem

    bool DisplayDeltaTime(UserLayerFlags flags = 0);
    bool DisplayDeltaTime(const Window *window, UserLayerFlags flags = 0);

    bool EnableImGui(i32 configFlags = 0);
    bool EnableImGui(const Window *window, i32 configFlags = 0);

    bool DisableImGui(const Window *window = nullptr);

    bool ReloadImGui(i32 configFlags = 0);
    bool ReloadImGui(const Window *window, i32 configFlags = 0);

    i32 GetImGuiConfigFlags(const Window *window = nullptr) const
    {
        const WindowData *data = window ? getWindowData(window) : &m_MainWindow;
        return data->ImGuiConfigFlags;
    }
#endif
  private:
    template <std::derived_from<UserLayer> T, typename... LayerArgs>
    T *setUserLayer(WindowData &data, LayerArgs &&...args)
    {
        T *layer = new T(this, data.Window, std::forward<LayerArgs>(args)...);
        if (checkFlags(ApplicationFlag_Defer))
        {
            delete data.StagedLayer;
            data.StagedLayer = layer;
            data.SetFlags(ApplicationFlag_MustDestroyLayer | ApplicationFlag_MustReplaceLayer);
        }
        else
        {
            delete data.Layer;
            data.Layer = layer;
            data.ClearFlags(ApplicationFlag_MustDestroyLayer | ApplicationFlag_MustReplaceLayer);
        }
        return layer;
    }
    bool checkFlags(const ApplicationFlags flags) const
    {
        return m_Flags & flags;
    }
    void setFlags(const ApplicationFlags flags)
    {
        m_Flags |= flags;
    }
    void clearFlags(const ApplicationFlags flags)
    {
        m_Flags &= ~flags;
    }

    WindowData *getWindowData(const Window *window);
    const WindowData *getWindowData(const Window *window) const;

    void setUpdateDeltaTime(TKit::Timespan target, WindowData &data);
    void setRenderDeltaTime(TKit::Timespan target, WindowData &data);

    void processWindow(WindowData &data);
    WindowData createWindow(const WindowSpecs &specs);
    void destroyWindow(WindowData &data);
    void syncDeferredOperations();

    void closeAllWindows();
#ifdef __ONYX_MULTI_WINDOW
    Window *openWindow(const WindowSpecs &specs);
#endif

#ifdef ONYX_ENABLE_IMGUI
    bool enableImGui(WindowData &data, i32 flags);
    bool reloadImGui(WindowData &data, i32 flags);
    static void applyTheme(const WindowData &data);
#endif

    TKit::BlockAllocator m_WindowAllocator = TKit::BlockAllocator::CreateFromType<Window>(MaxWindows);

    WindowData m_MainWindow{};
#ifdef __ONYX_MULTI_WINDOW
    TKit::StaticArray<WindowData, MaxWindows - 1> m_Windows;
    TKit::StaticArray<WindowSpecs, MaxWindows - 1> m_WindowsToAdd;
#endif

    TKit::Timespan m_DeltaTime{};
    ApplicationFlags m_Flags = 0;
};

} // namespace Onyx
