#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"
#include "kit/profiling/clock.hpp"

#include <atomic>

namespace ONYX
{
class IDrawable;
enum class MultiWindowFlow
{
    SERIAL = 0,
    CONCURRENT = 1
};

// Notes (to be added to docs later)
// OnEvent: in window opened events, concurrent mode executes it once, through the thread (window) that issued the
// OpenWindow call

// serial mode process the event as any other for the corresponding window (for each layer. mark it as processed by
// returning true)

class IApplication
{
    KIT_NON_COPYABLE(IApplication)
  public:
    IApplication() = default;
    virtual ~IApplication() noexcept;

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
    usize GetWindowCount() const noexcept;

    f32 GetDeltaTime() const noexcept;
    virtual MultiWindowFlow GetMultiWindowFlow() const noexcept = 0;

    virtual void Startup() noexcept;
    void Shutdown() noexcept;
    bool NextFrame(KIT::Clock &p_Clock) noexcept;

    void Run() noexcept;

    void Draw(IDrawable &p_Drawable, usize p_WindowIndex = 0) noexcept;

    bool Started() const noexcept;
    bool Terminated() const noexcept;

    LayerSystem Layers{this};

  protected:
    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    KIT::Ref<Device> m_Device;
    DynamicArray<KIT::Scope<Window>> m_Windows;
    bool m_MainThreadProcessing = false;

  private:
    virtual Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept = 0;
    virtual void processWindows() noexcept = 0;

    void createImGuiPool() noexcept;

    bool m_Started = false;
    bool m_Terminated = false;
    std::atomic<f32> m_DeltaTime = 0.f;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};

// There are two ways available to manage multiple windows in an ONYX application:
// - SERIAL: The windows are managed in a serial way, meaning that the windows are drawn one after the other in the main
// thread. This is the default and most forgiving mode, allowing submission of draw calls to multiple windows from the
// main thread (even another window itself)
// - CONCURRENT: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to submit draw calls to a window from the same thread that created
// the window

template <MultiWindowFlow Flow = MultiWindowFlow::SERIAL> class ONYX_API Application;

template <> class ONYX_API Application<MultiWindowFlow::SERIAL> final : public IApplication
{
    KIT_NON_COPYABLE(Application)
  public:
    using IApplication::IApplication;
    void Draw(Window &p_Window, usize p_WindowIndex = 0) noexcept;
    void CloseWindow(usize p_Index) noexcept override;
    MultiWindowFlow GetMultiWindowFlow() const noexcept override;

  private:
    Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept override;
    void processWindows() noexcept override;
};

template <> class ONYX_API Application<MultiWindowFlow::CONCURRENT> final : public IApplication
{
    KIT_NON_COPYABLE(Application)
  public:
    using IApplication::IApplication;
    void CloseWindow(usize p_Index) noexcept override;
    MultiWindowFlow GetMultiWindowFlow() const noexcept override;

    void Startup() noexcept override;

  private:
    Window *handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept override;
    void processWindows() noexcept override;
    KIT::Ref<KIT::Task<void>> createWindowTask(usize p_WindowIndex) noexcept;

    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;
    const std::thread::id m_MainThreadID = std::this_thread::get_id();
};
} // namespace ONYX