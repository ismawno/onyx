#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
class Perspective3D final : public Camera3D
{
  public:
    Perspective3D() = default;
    Perspective3D(f32 p_Aspect, f32 p_FieldOfView, const glm::mat3 &p_Rotation = glm::mat3(1.f), f32 p_Near = 0.1f,
                  f32 p_Far = 10.f);
    Perspective3D(const glm::vec3 &position, f32 p_Aspect, f32 p_FieldOfView,
                  const glm::mat3 &p_Rotation = glm::mat3(1.f), f32 p_Near = 0.1f, f32 p_Far = 10.f);

    f32 Near;
    f32 Far;
    f32 FieldOfView;

    void UpdateMatrices() noexcept override;
    void KeepAspectRatio(f32 p_Aspect) noexcept override;

  private:
    f32 m_Aspect;
};
} // namespace ONYX