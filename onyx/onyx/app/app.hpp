#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/app/theme.hpp"
#endif
#include "tkit/profiling/clock.hpp"

#ifndef ONYX_APP_MAX_WINDOWS
#    define ONYX_APP_MAX_WINDOWS 8
#endif

#if ONYX_APP_MAX_WINDOWS < 1
#    error "[ONYX] Onyx maximum windows must be at least 1";
#endif

#if ONYX_APP_MAX_WINDOWS > 1
#    define __ONYX_MULTI_WINDOW
#endif

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
class ONYX_API Application
{
  public:
    using Flags = u8;
    enum FlagBit : Flags
    {
#ifdef ONYX_ENABLE_IMGUI
        Flag_MustEnableImGui = 1 << 0,
        Flag_MustDisableImGui = 1 << 1,
        Flag_ImGuiRunning = 1 << 2,
        Flag_ImGuiEnabled = 1 << 3,
#endif
        Flag_Defer = 1 << 4,
        Flag_Quit = 1 << 5,
        Flag_MustDestroyLayer = 1 << 6,
        Flag_MustReplaceLayer = 1 << 7
    };

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

#ifdef ONYX_ENABLE_IMGUI
    struct ImGuiFlags
    {
        i32 Config = 0;
        i32 Backend = 0;
    };
#endif

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
        ImGuiFlags ImGuiFlags{};
        Theme *Theme = nullptr;
        // only relevant when displaying delta times, not for actual logic
        bool MirrorDeltas = true;
#endif
        Flags Flags = 0;

        bool CheckFlags(const Application::Flags p_Flags) const
        {
            return Flags & p_Flags;
        }
        void SetFlags(const Application::Flags p_Flags)
        {
            Flags |= p_Flags;
        }
        void ClearFlags(const Application::Flags p_Flags)
        {
            Flags &= ~p_Flags;
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

    Application(const WindowSpecs &p_MainSpecs) : m_MainWindow(createWindow(p_MainSpecs))
    {
        if (p_MainSpecs.CreationCallback)
            p_MainSpecs.CreationCallback(m_MainWindow.Window);
    }
    Application(const Window::Specs &p_MainSpecs = {}) : Application({.Specs = p_MainSpecs})
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
     * @param p_Clock A clock that lets both the API and the user to keep track of the frame time.
     * @return Whether the application must keep running.
     */
    bool NextFrame(TKit::Clock &p_Clock);

#ifdef ONYX_ENABLE_IMGUI
    /**
     * @brief Set an object derived from `Theme` to apply an ImGui theme to a given window.
     *
     * @tparam T User defined theme.
     * @tparam ThemeArgs Arguments to pass to the theme constructor.
     * @param p_Window The window for which the ImGui theme will be applied to.
     * @param p_Args Arguments to pass to the theme constructor.
     * @return Pointer to the theme object.
     */
    template <std::derived_from<Theme> T, typename... ThemeArgs>
    T *SetTheme(const Window *p_Window, ThemeArgs &&...p_Args)
    {
        WindowData *data = getWindowData(p_Window);
        if (data->Theme)
            delete data->Theme;

        T *theme = new T{std::forward<ThemeArgs>(p_Args)...};
        data->Theme = theme;
        applyTheme(*data);
        return theme;
    }
    /**
     * @brief Set an object derived from `Theme` to apply an ImGui theme to the main window.
     *
     * @tparam T User defined theme.
     * @tparam ThemeArgs Arguments to pass to the theme constructor.
     * @param p_Args Arguments to pass to the theme constructor.
     * @return Pointer to the theme object.
     */
    template <std::derived_from<Theme> T, typename... ThemeArgs> T *SetTheme(ThemeArgs &&...p_Args)
    {
        if (m_MainWindow.Theme)
            delete m_MainWindow.Theme;

        T *theme = new T{std::forward<ThemeArgs>(p_Args)...};
        m_MainWindow.Theme = theme;
        applyTheme(m_MainWindow);
        return theme;
    }

    /**
     * @brief Apply the current theme to `ImGui`. Use `SetTheme()` to set a new theme.
     *
     */
    void ApplyTheme(const Window *p_Window = nullptr);
#endif

    /**
     * @brief Set a new user layer to provide custom functionality.
     *
     * This method will delete the current user layer for the given window and replace it with a new one. If called in
     * the middle of a frame, the operation will be deferred until the end of the frame.
     *
     * @tparam T User defined layer.
     * @tparam LayerArgs Arguments to pass to the layer constructor.
     * @param p_Window The window for which to set the user layer.
     * @param p_Args Arguments to pass to the layer constructor.
     * @return Pointer to the new user layer.
     */
    template <std::derived_from<UserLayer> T, typename... LayerArgs>
    T *SetUserLayer(Window *p_Window, LayerArgs &&...p_Args)
    {
        WindowData *data = getWindowData(p_Window);
        return setUserLayer<T>(*data, std::forward<LayerArgs>(p_Args)...);
    }
    /**
     * @brief Set a new user layer to provide custom functionality.
     *
     * This method will delete the current user layer for the main window and replace it with a new one. If called in
     * the middle of a frame, the operation will be deferred until the end of the frame.
     *
     * @tparam T User defined layer.
     * @tparam LayerArgs Arguments to pass to the layer constructor.
     * @param p_Args Arguments to pass to the layer constructor.
     * @return Pointer to the new user layer.
     */
    template <std::derived_from<UserLayer> T, typename... LayerArgs> T *SetUserLayer(LayerArgs &&...p_Args)
    {
        return setUserLayer<T>(m_MainWindow, std::forward<LayerArgs>(p_Args)...);
    }

    /**
     * @brief Remove a user layer without destroying the resource, detaching it from the application and releasing its
     * ownership.
     *
     * This method will remove the current user layer for the given window. Ownership will be returned to the caller. If
     * called in the middle of a frame, the operation will be deferred until the end of the frame. In those cases, it is
     * UB to delete the resource until the current frame has finished.
     *
     * @tparam T User defined layer.
     * @param p_Window The target window. If nullptr is passed, the main window will be affected.
     * @return Pointer to the removed user layer.
     */
    template <std::derived_from<UserLayer> T = UserLayer> T *RemoveUserLayer(const Window *p_Window = nullptr)
    {
        WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        UserLayer *layer = data->Layer;
        if (checkFlags(Flag_Defer))
            data->SetFlags(Flag_MustReplaceLayer);
        else
            data->Layer = nullptr;
        return static_cast<T>(layer);
    }

    /**
     * @brief Destroy a user layer, freeing the resource and detaching it from the application.
     *
     * This method will destroy the current user layer for the given window. If called in the middle of a frame, the
     * operation will be deferred until the end of the frame.
     *
     * @tparam T User defined layer.
     * @param p_Window The target window. If nullptr is passed, the main window will be affected.
     */
    template <std::derived_from<UserLayer> T = UserLayer> void DestroyUserLayer(const Window *p_Window = nullptr)
    {
        WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        if (checkFlags(Flag_Defer))
            data->SetFlags(Flag_MustDestroyLayer);
        else
        {
            delete data->Layer;
            data->Layer = nullptr;
            data->ClearFlags(Flag_MustDestroyLayer);
        }
    }

    template <std::derived_from<UserLayer> T = UserLayer> const T *GetUserLayer(const Window *p_Window = nullptr) const
    {
        const WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        return static_cast<T>(data->Layer);
    }
    template <std::derived_from<UserLayer> T = UserLayer> T *GetUserLayer(const Window *p_Window = nullptr)
    {
        const WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        return static_cast<T>(data->Layer);
    }

    /**
     * @brief Check if certain operations must wait until the end of the current frame to be executed.
     *
     */
    bool MustDefer() const
    {
        return checkFlags(Flag_Defer);
    }

#ifdef __ONYX_MULTI_WINDOW
    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     *
     * @param p_Specs The specification of the window to create.
     * @return A window handle if the operation ran immediately, nullptr otherwise.
     */
    Window *OpenWindow(const WindowSpecs &p_Specs)
    {
        if (checkFlags(Flag_Defer))
        {
            m_WindowsToAdd.Append(p_Specs);
            return nullptr;
        }
        return openWindow(p_Specs);
    }

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     *
     * @param p_Specs The specification of the window to create.
     * @return A window handle if the operation ran immediately, nullptr otherwise.
     */
    Window *OpenWindow(const Window::Specs &p_Specs = {})
    {
        return OpenWindow(WindowSpecs{.Specs = p_Specs});
    }

    /**
     * @brief Close the given window.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the window's layer destructor, if any.
     *
     * @param p_Window The window to close.
     * @return Whether the operation could execute immediately.
     */
    bool CloseWindow(Window *p_Window);
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

    const DeltaTime &GetRenderDeltaTime(const Window *p_Window = nullptr) const
    {
        const WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        return data->RenderClock.Delta.Time;
    }
    const DeltaTime &GetUpdateDeltaTime(const Window *p_Window = nullptr) const
    {
        const WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        return data->UpdateClock.Delta.Time;
    }

    void SetDeltaTime(const TKit::Timespan p_Target, const Window *p_Window = nullptr)
    {
        WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        setRenderDeltaTime(p_Target, *data);
        setUpdateDeltaTime(p_Target, *data);
    }
    void SetRenderDeltaTime(const TKit::Timespan p_Target, const Window *p_Window = nullptr)
    {
        WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        setRenderDeltaTime(p_Target, *data);
    }
    void SetUpdateDeltaTime(const TKit::Timespan p_Target, const Window *p_Window = nullptr)
    {
        WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        setUpdateDeltaTime(p_Target, *data);
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

    bool DisplayDeltaTime(UserLayer::Flags p_Flags = 0);
    bool DisplayDeltaTime(const Window *p_Window, UserLayer::Flags p_Flags = 0);

    bool EnableImGui(ImGuiFlags p_Flags = {0, 0});
    bool EnableImGui(const Window *p_Window, ImGuiFlags p_Flags = {0, 0});

    bool DisableImGui(const Window *p_Window = nullptr);

    bool ReloadImGui(ImGuiFlags p_Flags = {0, 0});
    bool ReloadImGui(const Window *p_Window, ImGuiFlags p_Flags = {0, 0});

    ImGuiFlags GetImGuiFlags(const Window *p_Window = nullptr) const
    {
        const WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
        return data->ImGuiFlags;
    }
#endif
  private:
    template <std::derived_from<UserLayer> T, typename... LayerArgs>
    T *setUserLayer(WindowData &p_Data, LayerArgs &&...p_Args)
    {
        T *layer = new T(this, p_Data.Window, std::forward<LayerArgs>(p_Args)...);
        if (checkFlags(Flag_Defer))
        {
            delete p_Data.StagedLayer;
            p_Data.StagedLayer = layer;
            p_Data.SetFlags(Flag_MustDestroyLayer | Flag_MustReplaceLayer);
        }
        else
        {
            delete p_Data.Layer;
            p_Data.Layer = layer;
            p_Data.ClearFlags(Flag_MustDestroyLayer | Flag_MustReplaceLayer);
        }
        return layer;
    }
    bool checkFlags(const Flags p_Flags) const
    {
        return m_Flags & p_Flags;
    }
    void setFlags(const Flags p_Flags)
    {
        m_Flags |= p_Flags;
    }
    void clearFlags(const Flags p_Flags)
    {
        m_Flags &= ~p_Flags;
    }

    WindowData *getWindowData(const Window *p_Window);
    const WindowData *getWindowData(const Window *p_Window) const;

    void setUpdateDeltaTime(TKit::Timespan p_Target, WindowData &p_Data);
    void setRenderDeltaTime(TKit::Timespan p_Target, WindowData &p_Data);

    void processWindow(WindowData &p_Data);
    WindowData createWindow(const WindowSpecs &p_Specs);
    void destroyWindow(WindowData &p_Data);
    void syncDeferredOperations();

    void closeAllWindows();
#ifdef __ONYX_MULTI_WINDOW
    Window *openWindow(const WindowSpecs &p_Specs);
#endif

#ifdef ONYX_ENABLE_IMGUI
    bool enableImGui(WindowData &p_Data, ImGuiFlags p_Flags);
    bool reloadImGui(WindowData &p_Data, ImGuiFlags p_Flags);
    static void applyTheme(const WindowData &p_Data);
#endif

    TKit::BlockAllocator m_WindowAllocator = TKit::BlockAllocator::CreateFromType<Window>(ONYX_APP_MAX_WINDOWS);

    WindowData m_MainWindow{};
#ifdef __ONYX_MULTI_WINDOW
    TKit::StaticArray<WindowData, ONYX_APP_MAX_WINDOWS - 1> m_Windows;
    TKit::StaticArray<WindowSpecs, ONYX_APP_MAX_WINDOWS - 1> m_WindowsToAdd;
#endif

    TKit::Timespan m_DeltaTime{};

    Flags m_Flags = 0;
};

} // namespace Onyx
