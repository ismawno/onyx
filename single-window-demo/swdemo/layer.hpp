#pragma once

#include "onyx/app/layer.hpp"
#include "utils/window_data.hpp"

namespace ONYX
{
class Application;
class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender() noexcept override;
    bool OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application = nullptr;
    WindowData m_Data;
};
} // namespace ONYX