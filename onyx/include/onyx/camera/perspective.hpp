#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
class ONYX_API Perspective3D final : public Camera<3>
{
  public:
    Perspective3D(f32 p_Aspect = ONYX_DEFAULT_ASPECT, f32 p_FieldOfView = glm::radians(75.f), f32 p_Near = 0.1f,
                  f32 p_Far = 10.f);

    f32 Near;
    f32 Far;
    f32 FieldOfView;

    mat4 ComputeProjectionView() const noexcept override;
    mat4 ComputeInverseProjectionView() const noexcept override;

    void SetAspectRatio(f32 p_Aspect) noexcept override;

    bool IsOrthographic() const noexcept override;

  private:
    f32 m_Aspect;
};

} // namespace ONYX