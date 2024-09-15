#pragma once

#include "onyx/app/window.hpp"

namespace ONYX
{
class Drawable;

// There are two ways available to manage multiple windows in an ONYX application:
// - SERIAL: The windows are managed in a serial way, meaning that the windows are drawn one after the other in the main
// thread. This is the default and most forgiving mode, allowing submission of draw calls to multiple windows from the
// main thread (even another window itself)
// - CONCURRENT: The windows are managed in a concurrent way, meaning that the windows are drawn in parallel. This mode
//  *can* be more efficient but requires the user to submit draw calls to a window from the same thread that created
// the window

enum class MultiWindowFlow
{
    SERIAL = 0,
    CONCURRENT = 1
};

template <MultiWindowFlow Flow = MultiWindowFlow::SERIAL> class ONYX_API Application
{
    KIT_NON_COPYABLE(Application)
  public:
    Application() noexcept = default;
    ~Application() noexcept;

    template <std::derived_from<ICamera> T, typename... CameraArgs>
    Window *OpenWindow(const Window::Specs &p_Specs, CameraArgs &&...p_Args) noexcept
    {
        Window *window = openWindow(p_Specs);
        window->SetCamera<T>(std::forward<CameraArgs>(p_Args)...);
        return window;
    }
    template <std::derived_from<ICamera> T, typename... CameraArgs> Window *OpenWindow(CameraArgs &&...p_Args) noexcept
    {
        const Window::Specs specs{};
        return OpenWindow<T>(specs, std::forward<CameraArgs>(p_Args)...);
    }

    void Draw(Drawable &p_Drawable, usize p_WindowIndex = 0) noexcept;
    void Draw(Window &p_Window, usize p_WindowIndex = 0) noexcept;

    void CloseWindow(usize p_Index) noexcept;
    void CloseWindow(const Window *p_Window) noexcept;

    void Start() noexcept;
    void Shutdown() noexcept;
    bool NextFrame() noexcept;

    void Run() noexcept;

  private:
    template <MultiWindowFlow WFlow> struct WindowData;
    template <> struct WindowData<MultiWindowFlow::SERIAL>
    {
        KIT::Scope<Window> Window;
    };
    template <> struct WindowData<MultiWindowFlow::CONCURRENT>
    {
        KIT::Scope<Window> Window;
        KIT::Ref<KIT::Task<void>> Task;
    };

    Window *openWindow(const Window::Specs &p_Specs) noexcept;
    void createImGuiPool() noexcept;
    void runAndManageWindows() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    DynamicArray<WindowData<Flow>> m_WindowData;
    KIT::Ref<Device> m_Device;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};
} // namespace ONYX