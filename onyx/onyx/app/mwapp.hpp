#pragma once

#include "onyx/app/app.hpp"

#include <atomic>

namespace ONYX
{
/**
 * @brief The window threading scheme a multi-window application can use. Serial mode is the default and most
 * forgiving/user-friendly. It runs all windows in the main thread, sequentially. Concurrent mode runs all windows in
 * parallel, which can be beneficial in some scenarios (up to the user to decide), but can be more challenging to get
 * right.
 *
 */
enum WindowThreading
{
    Serial = 0,
    Concurrent = 1
};

// you are supposed to draw only 2D or 3D objects in a window, not both. you can still do it, but results may be weird
// (light behaves differently, etc)

// OnImGuiRender always runs in the main thread, and thus parallel to all other threads. If you want to display data
// from window #4 with imgui, you may want to use some kind of synchronization mechanism if using concurrent mode

/**
 * @brief The base class of a multi-window application.
 *
 * This class provides a simple interface and partial implementation for a multi-window application, which can either be
 * a serial or concurrent application. The user can choose the threading scheme by specifying the template parameter in
 * the MultiWindowApplication class. Because of the ability of having multiple windows, the user must always explicitly
 * open windows from the application's API, including the main (first) window.
 *
 * To better manage window lifetimes, calls to OpenWindow() or CloseWindow() may be deferred if called from within an
 * ongoing frame. Never update your state based on the calls of these functions, but rather react to the corresponding
 * events (WindowOpened, WindowClosed) to ensure synchronization between the API and the user.
 *
 */
class IMultiWindowApplication : public IApplication
{
    KIT_NON_COPYABLE(IMultiWindowApplication)
  public:
    IMultiWindowApplication() = default;

    /**
     * @brief Open a new window with the given specs.
     *
     * @note The window addition may not take effect immediately if called in the middle of a frame. Only react to
     * the window addition through the corresponding event (WindowOpened) unless you are sure that the window is being
     * added outside the frame loop.
     *
     * @param p_Specs
     */
    virtual void OpenWindow(const Window::Specs &p_Specs = {}) noexcept = 0;

    /**
     * @brief Close the window at the given index.
     *
     * @note The window removal may not take effect immediately if called in the middle of a frame. Only react to
     * the window removal through the corresponding event (WindowClosed) unless you are sure that the window is being
     * removed outside the frame loop.
     *
     * @param p_Index The index of the window to close.
     */
    virtual void CloseWindow(usize p_Index) noexcept = 0;

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
     * @return const Window* Pointer to the window at the given index.
     */
    const Window *GetWindow(usize p_Index) const noexcept;

    /**
     * @brief Get a pointer to the window at the specified index.
     *
     * @param p_Index The index of the window to retrieve.
     * @return Window* Pointer to the window at the given index.
     */
    Window *GetWindow(usize p_Index) noexcept;

    /**
     * @brief Get a pointer to the main window.
     *
     * @return const Window* Pointer to the main window.
     */
    const Window *GetMainWindow() const noexcept override;

    /**
     * @brief Get a pointer to the main window.
     *
     * @return Window* Pointer to the main window.
     */
    Window *GetMainWindow() noexcept override;

    /**
     * @brief Get the number of currently open windows.
     *
     * @return usize The number of windows.
     */
    usize GetWindowCount() const noexcept;

    /**
     * @brief Get the threading mode used by the application.
     *
     * @return WindowThreading The threading mode (Serial or Concurrent).
     */
    virtual WindowThreading GetWindowThreading() const noexcept = 0;

    /**
     * @brief Proceed to the next frame of the application.
     *
     * @param p_Clock The clock used to measure frame time.
     * @return true if the application should continue running, false otherwise.
     */
    bool NextFrame(KIT::Clock &p_Clock) noexcept override;

  protected:
    /// The list of currently open windows.
    DynamicArray<KIT::Scope<Window>> m_Windows;

  private:
    /**
     * @brief Process window events and rendering for each window.
     *
     */
    virtual void processWindows() noexcept = 0;

    /**
     * @brief Set the delta time between frames.
     *
     * @param p_DeltaTime The delta time to set.
     */
    virtual void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept = 0;
};

// There are two ways available to manage multiple windows in an ONYX application:
// - Serial: The windows are managed in a serial way, meaning that the windows are drawn one after the other in the main
// thread. This is the default and most forgiving mode, allowing submission of onyx draw calls to multiple windows from
// the main thread
// - Concurrent: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to take extra care when submitting onyx draw calls to multiple windows

template <WindowThreading Threading = Serial> class ONYX_API MultiWindowApplication;

/**
 * @brief A multi-window application that manages all windows in the main thread, sequentially.
 *
 * This class is the default implementation of a multi-window application.
 *
 */
template <> class ONYX_API MultiWindowApplication<Serial> final : public IMultiWindowApplication
{
    KIT_NON_COPYABLE(MultiWindowApplication)
  public:
    MultiWindowApplication() = default;

    void OpenWindow(const Window::Specs &p_Specs = {}) noexcept override;
    void CloseWindow(usize p_Index) noexcept override;

    /**
     * @brief Get the threading mode used by the application.
     *
     * @return WindowThreading The threading mode (Serial).
     */
    WindowThreading GetWindowThreading() const noexcept override;

    /**
     * @brief Get the time elapsed between the current and previous frame.
     *
     * @return KIT::Timespan The delta time.
     */
    KIT::Timespan GetDeltaTime() const noexcept override;

  private:
    /**
     * @brief Process windows in serial mode.
     *
     */
    void processWindows() noexcept override;

    /**
     * @brief Set the delta time between frames.
     *
     * @param p_DeltaTime The delta time to set.
     */
    void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept override;

    /// The time elapsed between frames.
    KIT::Timespan m_DeltaTime;

    /// Specifications of windows to add in the next frame.
    DynamicArray<Window::Specs> m_WindowsToAdd;

    /// Indicates whether window management should be deferred.
    bool m_MustDeferWindowManagement = false;
};

/**
 * @brief A multi-window application that manages all windows in parallel.
 *
 * This mode can be more efficient than the serial mode, but requires the user to take extra care when their code is
 * executed through layer methods.
 *
 */
template <> class ONYX_API MultiWindowApplication<Concurrent> final : public IMultiWindowApplication
{
    KIT_NON_COPYABLE(MultiWindowApplication)
  public:
    using IMultiWindowApplication::IMultiWindowApplication;

    void OpenWindow(const Window::Specs &p_Specs = {}) noexcept override;
    void CloseWindow(usize p_Index) noexcept override;

    /**
     * @brief Get the threading mode used by the application.
     *
     * @return WindowThreading The threading mode (Concurrent).
     */
    WindowThreading GetWindowThreading() const noexcept override;

    /**
     * @brief Get the time elapsed between the current and previous frame.
     *
     * @return KIT::Timespan The delta time.
     */
    KIT::Timespan GetDeltaTime() const noexcept override;

    /**
     * @brief Perform startup tasks for the application.
     *
     */
    void Startup() noexcept override;

  private:
    /**
     * @brief Process windows in concurrent mode.
     *
     */
    void processWindows() noexcept override;

    /**
     * @brief Create a task for processing a window in concurrent mode.
     *
     * @param p_WindowIndex The index of the window to process.
     * @return KIT::Ref<KIT::Task<void>> A reference to the created task.
     */
    KIT::Ref<KIT::Task<void>> createWindowTask(usize p_WindowIndex) noexcept;

    /**
     * @brief Set the delta time between frames.
     *
     * @param p_DeltaTime The delta time to set.
     */
    void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept override;

    /// Tasks for processing windows in concurrent mode.
    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;

    /// The time elapsed between frames, shared between threads.
    std::atomic<KIT::Timespan> m_DeltaTime;

    /// Specifications of windows to add in the next frame.
    DynamicArray<Window::Specs> m_WindowsToAdd;

    /// Indicates whether window management should be deferred.
    bool m_MustDeferWindowManagement = false;

    /// The ID of the main thread.
    const std::thread::id m_MainThreadID = std::this_thread::get_id();

    /// Mutex for synchronizing access to m_WindowsToAdd.
    mutable std::mutex m_WindowsToAddMutex;
};
} // namespace ONYX
