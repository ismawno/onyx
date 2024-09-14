#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/drawing/transform.hpp"

namespace ONYX
{
class ICamera ONYX_API
{
  public:
    virtual ~ICamera() = default;

    virtual void UpdateMatrices() noexcept = 0;
    virtual void KeepAspectRatio(f32 p_Aspect) noexcept = 0;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API Camera : public ICamera
{
  public:
    virtual void KeepAspectRatio(f32 p_Aspect) noexcept;

    vec<N> ScreenToWorld(const vec2 &p_Screen) const noexcept;
    vec2 WorldToScreen(const vec<N> &p_World) const noexcept;

    const mat4 &GetProjection() const noexcept;
    const mat4 &GetInverseProjection() const noexcept;

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