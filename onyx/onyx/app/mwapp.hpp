#pragma once

#include "onyx/app/app.hpp"

#include <atomic>

namespace ONYX
{
/**
 * @brief The window threading scheme a multi-window application can use. Serial mode is the default and most
 * forgiving/user-friendly. It runs all windows in the main thread, sequentially. Concurrent mode runs all windows in
 * parallel, which can be beneficial in some scenarios (up to the user to decide), but can be more challenging to get
 * right
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

    const Window *GetWindow(usize p_Index) const noexcept;
    Window *GetWindow(usize p_Index) noexcept;

    const Window *GetMainWindow() const noexcept override;
    Window *GetMainWindow() noexcept override;

    usize GetWindowCount() const noexcept;
    virtual WindowThreading GetWindowThreading() const noexcept = 0;

    bool NextFrame(KIT::Clock &p_Clock) noexcept override;

  protected:
    DynamicArray<KIT::Scope<Window>> m_Windows;

  private:
    virtual void processWindows() noexcept = 0;
    virtual void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept = 0;
};

// There are two ways available to manage multiple windows in an ONYX application:
// - Serial: The windows are managed in a serial way, meaning that the windows are drawn one after the other in the main
// thread. This is the default and most forgiving mode, allowing submission of draw calls to multiple windows from the
// main thread
// - Concurrent: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to take extra care when submitting draw calls to multiple windows

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

    WindowThreading GetWindowThreading() const noexcept override;
    KIT::Timespan GetDeltaTime() const noexcept override;

  private:
    void processWindows() noexcept override;
    void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept override;

    KIT::Timespan m_DeltaTime;
    DynamicArray<Window::Specs> m_WindowsToAdd;
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
    WindowThreading GetWindowThreading() const noexcept override;
    KIT::Timespan GetDeltaTime() const noexcept override;

    void Startup() noexcept override;

  private:
    void processWindows() noexcept override;
    KIT::Ref<KIT::Task<void>> createWindowTask(usize p_WindowIndex) noexcept;
    void setDeltaTime(KIT::Timespan p_DeltaTime) noexcept override;

    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;

    std::atomic<KIT::Timespan> m_DeltaTime;
    DynamicArray<Window::Specs> m_WindowsToAdd;
    bool m_MustDeferWindowManagement = false;
    const std::thread::id m_MainThreadID = std::this_thread::get_id();

    mutable std::mutex m_WindowsToAddMutex;
};
} // namespace ONYX