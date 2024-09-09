#pragma once

#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API Application
{
    KIT_NON_COPYABLE(Application)
  public:
    Application() noexcept = default;
    ~Application() noexcept;

    Window<N> *OpenWindow(const Window<N>::Specs &p_Specs) noexcept;
    Window<N> *OpenWindow() noexcept;

    void CloseWindow(usize p_Index) noexcept;
    void CloseWindow(const Window<N> *p_Window) noexcept;

    void Start() noexcept;
    void Shutdown() noexcept;
    bool NextFrame() noexcept;

    void Run() noexcept;

  private:
    struct WindowData
    {
        KIT::Scope<Window<N>> Window;
        KIT::Ref<KIT::Task<void>> Task;
    };

    void createImGuiPool() noexcept;
    void runAndManageWindows() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    void initializeImGui(Window<N> &p_Window) noexcept;
    void shutdownImGui() noexcept;

    DynamicArray<WindowData> m_WindowData;
    KIT::Ref<Device> m_Device;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};

using Application2D = Application<2>;
using Application3D = Application<3>;
} // namespace ONYX