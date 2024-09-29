#pragma once

#include "onyx/app/app.hpp"
#include "onyx/app/layer.hpp"
#include "onyx/draw/primitives/primitives.hpp"

namespace ONYX
{
enum CameraType : int
{
    ORTHOGRAPHIC2D = 0,
    ORTHOGRAPHIC3D,
    PERSPECTIVE3D
};

class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    // void OnRender() noexcept override;
    void OnImGuiRender() noexcept override;
    // bool OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application = nullptr;
    CameraType m_CameraType = CameraType::ORTHOGRAPHIC2D;
};
} // namespace ONYX