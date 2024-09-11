#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/model/transform.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Camera
{
  public:
    virtual ~Camera() = default;

    virtual void UpdateMatrices() noexcept = 0;
    virtual void KeepAspectRatio(f32 p_Aspect) noexcept;

    vec<N> ScreenToWorld(const vec2 &p_Screen) const noexcept;
    vec2 WorldToScreen(const vec<N> &p_World) const noexcept;

    const mat4 &Projection() const noexcept;
    const mat4 &InverseProjection() const noexcept;

    void FlipY() noexcept;

    ONYX::Transform<N> Transform;

  protected:
    mat4 m_Projection{1.f};
    mat4 m_InverseProjection{1.f};
    bool m_YFlipped = false;
};

using Camera2D = Camera<2>;
using Camera3D = Camera<3>;

} // namespace ONYX