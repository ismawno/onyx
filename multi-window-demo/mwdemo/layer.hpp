#pragma once

#include "onyx/app/user_layer.hpp"
#include "onyx/app/app.hpp"
#include "utils/window_data.hpp"

namespace Onyx::Demo
{
class MWExampleLayer final : public UserLayer
{
  public:
    MWExampleLayer(MultiWindowApplication *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnUpdate(u32 p_WindowIndex) noexcept override;
    void OnRender(u32 p_WindowIndex, VkCommandBuffer p_CommandBuffer) noexcept override;
    void OnImGuiRender() noexcept override;
    void OnEvent(u32 p_WindowIndex, const Event &p_Event) noexcept override;

  private:
    MultiWindowApplication *m_Application = nullptr;
    TKit::StaticArray8<WindowData> m_Data;
};
} // namespace Onyx::Demo