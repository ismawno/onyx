#pragma once

#include "onyx/app/layer.hpp"
#include "utils/window_data.hpp"

namespace Onyx
{
class Application;
class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnUpdate() noexcept override;
    void OnRender(VkCommandBuffer) noexcept override;
    bool OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application = nullptr;
    WindowData m_Data;
};
} // namespace Onyx