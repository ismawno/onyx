#pragma once

#include "onyx/app/layer.hpp"
#include "utils/window_data.hpp"

namespace Onyx
{
class IMultiWindowApplication;
class MWExampleLayer final : public Layer
{
  public:
    MWExampleLayer(IMultiWindowApplication *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender(usize p_WindowIndex, VkCommandBuffer) noexcept override;
    void OnImGuiRender() noexcept override;
    bool OnEvent(usize p_WindowIndex, const Event &p_Event) noexcept override;

  private:
    IMultiWindowApplication *m_Application = nullptr;
    TKit::StaticArray8<WindowData> m_Data;
};
} // namespace Onyx