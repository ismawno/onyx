#pragma once

#include "onyx/app/app.hpp"
#include "onyx/app/layer.hpp"

namespace ONYX
{
enum ProjectionType : int
{
    ORTHOGRAPHIC2D = 0,
    ORTHOGRAPHIC3D,
    PERSPECTIVE3D
};

class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnRender() noexcept override;
    void OnImGuiRender() noexcept override;

  private:
    ONYX_DIMENSION_TEMPLATE void renderPrimitiveSpawnUI(const Color &p_Color) noexcept;

    Application *m_Application = nullptr;
    ProjectionType m_ProjectionType = ProjectionType::ORTHOGRAPHIC2D;
};
} // namespace ONYX