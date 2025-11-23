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
    SWExampleLayer(Application *p_Application, Scene p_Scene);

    void OnUpdate() override;
    void OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer) override;
    void OnEvent(const Event &p_Event) override;

  private:
    Application *m_Application = nullptr;
    WindowData m_Data;
};
} // namespace Onyx::Demo
