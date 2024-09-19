#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
class ONYX_API Perspective final : public Camera<3>
{
  public:
    Perspective(f32 p_Aspect, f32 p_FieldOfView = glm::radians(75.f), const glm::mat3 &p_Rotation = glm::mat3(1.f),
                f32 p_Near = 0.1f, f32 p_Far = 10.f);
    Perspective(const glm::vec3 &position, f32 p_Aspect, f32 p_FieldOfView,
                const glm::mat3 &p_Rotation = glm::mat3(1.f), f32 p_Near = 0.1f, f32 p_Far = 10.f);

    f32 Near;
    f32 Far;
    f32 FieldOfView;

    void UpdateMatrices() noexcept override;
    void SetAspectRatio(f32 p_Aspect) noexcept override;

  private:
    f32 m_Aspect;
};

using Perspective3D = Perspective;
} // namespace ONYX