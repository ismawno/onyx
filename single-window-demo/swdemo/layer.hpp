#pragma once

#include "onyx/app/user_layer.hpp"
#include "utils/window_data.hpp"

namespace Onyx
{
class Application;
}

namespace Onyx::Demo
{
class SWExampleLayer final : public UserLayer
{
  public:
    SWExampleLayer(Application *p_Application, Scene p_Scene) noexcept;

    void OnStart() noexcept override;
    void OnUpdate() noexcept override;
    void OnFrameBegin(u32, VkCommandBuffer) noexcept override;
    void OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer) noexcept override;
    void OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application = nullptr;
    WindowData m_Data;
    Scene m_Scene;
};
} // namespace Onyx::Demo
