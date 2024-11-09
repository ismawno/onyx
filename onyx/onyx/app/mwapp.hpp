#pragma once

#include "onyx/app/app.hpp"

#include <atomic>

namespace ONYX
{
enum WindowThreading
{
    Serial = 0,
    Concurrent = 1
};

// Notes (to be added to docs later)
// When opening/closing a window, the corresponding event is sent as soon as the action takes place, to ensure
// synchronization between the API and the user

// an app can only be started and terminated ONCE

// shutdown can only be called one all windows have been closed and OUTSIDE the nextframe loop. It is only public bc of
// versatility

// in OnShutdown, it is not valid to reference any window any longer

// you are supposed to draw only 2D or 3D objects in a window, not both. you can still do it, but results may be weird
// (light behaves differently, etc)

// MUST ONLY RENDER IN OnRender, NOT ON UPDATE (will cause crashes)

// OnImGuiRender always runs in the main thread, and thus parallel to all other threads. If you want to display data
// from window #4 with imgui, you may want to use some kind of synchronization mechanism if using concurrent mode

class IMultiWindowApplication : public IApplication
{
    KIT_NON_COPYABLE(IMultiWindowApplication)
  public:
    IMultiWindowApplication() = default;

    virtual void OpenWindow(const Window::Specs &p_Specs = {}) noexcept = 0;

    virtual void CloseWindow(usize p_Index) noexcept = 0;
    void CloseWindow(const Window *p_Window) noexcept;
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
// main thread (even another window itself)
// - Concurrent: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to submit draw calls to a window from the same thread that created
// the window

template <WindowThreading Threading = Serial> class ONYX_API MultiWindowApplication;

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