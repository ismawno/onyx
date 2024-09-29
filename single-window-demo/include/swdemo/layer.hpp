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

    void OnRender() noexcept override;
    void OnImGuiRender() noexcept override;

  private:
    ONYX_DIMENSION_TEMPLATE void renderPrimitiveSpawnUI(const Color &p_Color) noexcept;

    Application *m_Application = nullptr;
    CameraType m_CameraType = CameraType::ORTHOGRAPHIC2D;
    DynamicArray<KIT::Scope<IDrawable>> m_Drawables;
};
} // namespace ONYX