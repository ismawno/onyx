#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/app/theme.hpp"
#endif
#include "tkit/profiling/clock.hpp"
#include "tkit/memory/ptr.hpp"

#ifndef ONYX_MAX_WINDOWS
#    define ONYX_MAX_WINDOWS 1
#endif

#if ONYX_MAX_WINDOWS < 1
#    error "[ONYX] Onyx maximum windows must be at least 1";
#endif

#if ONYX_MAX_WINDOWS > 1
#    define ONYX_MULTI_WINDOW
#endif

#ifdef ONYX_ENABLE_IMGUI
struct ImGuiContext;
#    ifdef ONYX_ENABLE_IMPLOT
struct ImPlotContext;
#    endif
#endif

namespace Onyx
{
/**
 * @brief This class provides a simple application interface.
 *
 * It can support multiple windows, with a main window that is always opened at construction. It is discouraged
 * (although technically possible) to use multiple applications in the same program. If you do so, note that the
 * following (among other possible unknown things) may work incorrectly:
 *
 * - ImGui: Currently, the application manages an ImGui context, and although that context is set at
 * the beginning of each frame to ensure consistency, ImGui itself discourages using multiple.
 *
 * - Draw call profile plot: The draw call count is global for the whole program, but it would be registered and reset
 * by each application in turn, yielding weird results.
 *
 * - `UserLayer::DisplayFrameTime()`: This method uses static variables. Using many applications would result in
 * inconsistencies when using said method.
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
    using Flags = u8;
    enum FlagBit : Flags
    {
#ifdef ONYX_ENABLE_IMGUI
        Flag_MustReloadImGui = 1 << 0,
        Flag_ImGuiRunning = 1 << 1,
#endif
        Flag_Defer = 1 << 2,
        Flag_Quit = 1 << 3,
        Flag_MustDestroyLayer = 1 << 4,
        Flag_MustReplaceLayer = 1 << 5
    };

#ifdef ONYX_MULTI_WINDOW
    struct BabyWindow
    {
        Window::Specs Specs{};
        std::function<void(Window *)> CreationCallback = nullptr;
    };
#endif

    struct WindowData
    {
        Window *Window = nullptr;
        UserLayer *Layer = nullptr;
        UserLayer *StagedLayer = nullptr;
#ifdef ONYX_ENABLE_IMGUI
        ImGuiContext *ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
        ImPlotContext *ImPlotContext = nullptr;
#    endif
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

  public:
    Application(const Window::Specs &p_Specs = {});
    ~Application();

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
     * @brief Apply the current theme to `ImGui`. Use `SetTheme()` to set a new theme.
     *
     */
    void ApplyTheme();
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
            data->Layer = nullptr;
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

#ifdef ONYX_MULTI_WINDOW
    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     *
     * @param p_Specs The specification of the window to create.
     * @param p_Callback A function callback that will execute as soon as the window is actually created.
     * @return Whether the operation could execute immediately.
     */
    bool OpenWindow(const Window::Specs &p_Specs, const std::function<void(Window *)> &p_Callback = nullptr);

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. If you need to
     * react to the window opening by, say, attaching a layer, use the provided callback argument.
     *
     * @param p_Specs
     * @return Whether the operation could execute immediately.
     */
    bool OpenWindow(const std::function<void(Window *)> &p_Callback = nullptr);

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
     * @brief Run the whole application in one go.
     *
     * This method automatically invokes an application loop, using `NextFrame()` under the hood.
     *
     */
    void Run();

    TKit::Timespan GetDeltaTime() const
    {
        return m_DeltaTime;
    }
#ifdef ONYX_ENABLE_IMGUI
    i32 GetImGuiConfigFlags() const
    {
        return m_ImGuiConfigFlags;
    }
    i32 GetImGuiBackendFlags() const
    {
        return m_ImGuiBackendFlags;
    }

    void SetImGuiConfigFlags(i32 p_Flags)
    {
        m_ImGuiConfigFlags = p_Flags;
    }
    void SetImGuiBackendFlags(i32 p_Flags)
    {
        m_ImGuiBackendFlags = p_Flags;
    }

    /**
     * @bried Reload ImGui, useful to modify the active flags.
     *
     * @return Whether the operation could execute immediately.
     */
    bool ReloadImGui(Window *p_Window);

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

    void processWindow(WindowData &p_Data);
    void destroyWindow(WindowData &p_Data);
    void closeAllWindows();
#ifdef ONYX_MULTI_WINDOW
    void openWindow(const BabyWindow &p_Baby);
#endif

#ifdef ONYX_ENABLE_IMGUI
    void initializeImGui(WindowData &p_Data);
    void shutdownImGui(WindowData &p_Data);
#endif

    WindowData m_MainWindow;
    TKit::Timespan m_DeltaTime;
    Flags m_Flags = 0;

#ifdef ONYX_MULTI_WINDOW
    TKit::StaticArray<WindowData, ONYX_MAX_WINDOWS - 1> m_Windows;
    TKit::StaticArray4<BabyWindow> m_WindowsToAdd;
#endif
    TKit::BlockAllocator m_WindowAllocator = TKit::BlockAllocator::CreateFromType<Window>(ONYX_MAX_WINDOWS);

#ifdef ONYX_ENABLE_IMGUI
    TKit::Scope<Theme> m_Theme;
    i32 m_ImGuiConfigFlags = 0;
    i32 m_ImGuiBackendFlags = 0;
#endif
};

} // namespace Onyx
