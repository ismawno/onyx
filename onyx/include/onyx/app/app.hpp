#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
class Drawable;

class IApplication
{
    KIT_NON_COPYABLE(IApplication)
  public:
    IApplication() = default;
    virtual ~IApplication() noexcept;

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

    virtual void CloseWindow(usize p_Index) noexcept = 0;
    void CloseWindow(const Window *p_Window) noexcept;
    void CloseAllWindows() noexcept;

    const Window *GetWindow(usize p_Index) const noexcept;
    Window *GetWindow(usize p_Index) noexcept;

    void Start() noexcept;
    void Shutdown() noexcept;
    bool NextFrame() noexcept;

    void Run() noexcept;

    void Draw(Drawable &p_Drawable, usize p_WindowIndex = 0) noexcept;

    LayerSystem Layers;

  protected:
    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    KIT::Ref<Device> m_Device;
    DynamicArray<KIT::Scope<Window>> m_Windows;

  private:
    virtual Window *openWindow(const Window::Specs &p_Specs) noexcept = 0;
    virtual void processWindows() noexcept = 0;

    void createImGuiPool() noexcept;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};

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

template <MultiWindowFlow Flow = MultiWindowFlow::SERIAL> class ONYX_API Application;

template <> class ONYX_API Application<MultiWindowFlow::SERIAL> final : public IApplication
{
    KIT_NON_COPYABLE(Application)
  public:
    using IApplication::IApplication;
    void Draw(Window &p_Window, usize p_WindowIndex = 0) noexcept;
    void CloseWindow(usize p_Index) noexcept override;

  private:
    Window *openWindow(const Window::Specs &p_Specs) noexcept override;
    void processWindows() noexcept override;
};

template <> class ONYX_API Application<MultiWindowFlow::CONCURRENT> final : public IApplication
{
    KIT_NON_COPYABLE(Application)
  public:
    using IApplication::IApplication;
    void CloseWindow(usize p_Index) noexcept override;

  private:
    Window *openWindow(const Window::Specs &p_Specs) noexcept override;
    void processWindows() noexcept override;

    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;
};
} // namespace ONYX