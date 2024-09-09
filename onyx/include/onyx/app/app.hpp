#pragma once

#include "onyx/app/window.hpp"

namespace ONYX
{
class ONYX_API Application
{
    KIT_NON_COPYABLE(Application)
  public:
    Application() noexcept = default;
    ~Application() noexcept;

    ONYX_DIMENSION_TEMPLATE Window<N> *OpenWindow(const Window<N>::Specs &p_Specs) noexcept;
    ONYX_DIMENSION_TEMPLATE Window<N> *OpenWindow() noexcept;

    Window2D *OpenWindow2D(const Window2D::Specs &p_Specs) noexcept;
    Window2D *OpenWindow2D() noexcept;

    Window3D *OpenWindow3D(const Window3D::Specs &p_Specs) noexcept;
    Window3D *OpenWindow3D() noexcept;

    void CloseWindow(usize p_Index) noexcept;
    void CloseWindow(const IWindow *p_Window) noexcept;

    void Start() noexcept;
    void Shutdown() noexcept;
    bool NextFrame() noexcept;

    void Run() noexcept;

  private:
    struct WindowData
    {
        KIT::Scope<IWindow> Window;
        KIT::Ref<KIT::Task<void>> Task;
    };

    void createImGuiPool() noexcept;
    void runAndManageWindows() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    void initializeImGui(IWindow &p_Window) noexcept;
    void shutdownImGui() noexcept;

    DynamicArray<WindowData> m_WindowData;
    KIT::Ref<Device> m_Device;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};
} // namespace ONYX