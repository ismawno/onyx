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
    void runAndManageWindows() noexcept;
    static void runFrame(Window<N> &p_Window) noexcept;

    DynamicArray<KIT::Scope<Window<N>>> m_Windows;
    DynamicArray<KIT::Ref<KIT::Task<void>>> m_Tasks;

    bool m_Started = false;
    bool m_Terminated = false;
};

using Application2D = Application<2>;
using Application3D = Application<3>;
} // namespace ONYX