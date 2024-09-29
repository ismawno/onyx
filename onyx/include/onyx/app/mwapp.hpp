#pragma once

#include "onyx/app/app.hpp"

#include <atomic>

namespace ONYX
{
class IDrawable;
enum class WindowFlow
{
    SERIAL = 0,
    CONCURRENT = 1
};

// Notes (to be added to docs later)
// OnEvent: in window opened events, concurrent mode executes it once, through the thread (window) that issued the
// OpenWindow call

// serial mode process the event as any other for the corresponding window (for each layer. mark it as processed by
// returning true)

// an app can only be started and terminated ONCE

// shutdown can only be called one all windows have been closed and OUTSIDE the nextframe loop. It is only public bc of
// versatility

// in OnShutdown, it is not valid to reference any window any longer

class IMultiWindowApplication : public IApplication
{
    KIT_NON_COPYABLE(IMultiWindowApplication)
  public:
    IMultiWindowApplication() = default;

    template <std::derived_from<ICamera> T, typename... CameraArgs>
    Window *OpenWindow(const Window::Specs &p_Specs, CameraArgs &&...p_Args) noexcept
    {
        auto window = KIT::Scope<Window>::Create(p_Specs);
        window->SetCamera<T>(std::forward<CameraArgs>(p_Args)...);
        return handleWindowAddition(std::move(window));
    }

    virtual void CloseWindow(usize p_Index) noexcept = 0;
    void CloseWindow(const Window *p_Window) noexcept;
    void CloseAllWindows() noexcept;

    const Window *GetWindow(usize p_Index) const noexcept;
    Window *GetWindow(usize p_Index) noexcept;

    const Window *GetMainWindow() const noexcept override;
    Window *GetMainWindow() noexcept override;

    usize GetWindowCount() const noexcept;

    f32 GetDeltaTime() const noexcept override;
    virtual WindowFlow GetWindowFlow() const noexcept = 0;

    bool NextFrame(KIT::Clock &p_Clock) noexcept override;

    void Draw(IDrawable &p_Drawable) noexcept override;
    void Draw(IDrawable &p_Drawable, usize p_WindowIndex) noexcept;

  protected:
    DynamicArray<KIT::Scope<Window>> m_Windows;
    bool m_MainThreadProcessing = false;

  private:
    virtual Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept = 0;
    virtual void processWindows() noexcept = 0;

    std::atomic<f32> m_DeltaTime = 0.f;
};

// There are two ways available to manage multiple windows in an ONYX application:
// - SERIAL: The windows are managed in a serial way, meaning that the windows are drawn one after the other in the main
// thread. This is the default and most forgiving mode, allowing submission of draw calls to multiple windows from the
// main thread (even another window itself)
// - CONCURRENT: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to submit draw calls to a window from the same thread that created
// the window

template <WindowFlow Flow = WindowFlow::SERIAL> class ONYX_API MultiWindowApplication;

template <> class ONYX_API MultiWindowApplication<WindowFlow::SERIAL> final : public IMultiWindowApplication
{
    KIT_NON_COPYABLE(MultiWindowApplication)
  public:
    MultiWindowApplication() = default;

    void CloseWindow(usize p_Index) noexcept override;
    WindowFlow GetWindowFlow() const noexcept override;

  private:
    Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept override;
    void processWindows() noexcept override;
};

template <> class ONYX_API MultiWindowApplication<WindowFlow::CONCURRENT> final : public IMultiWindowApplication
{
    KIT_NON_COPYABLE(MultiWindowApplication)
  public:
    using IMultiWindowApplication::IMultiWindowApplication;
    void CloseWindow(usize p_Index) noexcept override;
    WindowFlow GetWindowFlow() const noexcept override;

    void Startup() noexcept override;

  private:
    Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept override;
    void processWindows() noexcept override;
    KIT::Ref<KIT::Task<void>> createWindowTask(usize p_WindowIndex) noexcept;

    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;
    const std::thread::id m_MainThreadID = std::this_thread::get_id();
};
} // namespace ONYX