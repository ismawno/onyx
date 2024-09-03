#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
class Perspective3D final : public Camera3D
{
  public:
    Perspective3D() = default;
    Perspective3D(f32 aspect, f32 fovy, const glm::mat3 &rotation = glm::mat3(1.f), f32 near = 0.1f, f32 far = 10.f);
    Perspective3D(const glm::vec3 &position, f32 aspect, f32 fovy, const glm::mat3 &rotation = glm::mat3(1.f),
                  f32 near = 0.1f, f32 far = 10.f);

    f32 Near;
    f32 Far;
    f32 Fov;

    void UpdateMatrices() noexcept override;
    void KeepAspectRatio(f32 aspect) noexcept override;

  private:
    f32 m_aspect;
};
} // namespace ONYX