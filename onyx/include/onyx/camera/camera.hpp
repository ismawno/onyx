#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/draw/transform.hpp"

#define ONYX_DEFAULT_ASPECT (16.f / 9.f)

namespace ONYX
{
class ICamera ONYX_API
{
  public:
    virtual ~ICamera() = default;

    virtual void UpdateMatrices() noexcept = 0;
    virtual void SetAspectRatio(f32 p_Aspect) noexcept = 0;

    const mat4 &GetProjection() const noexcept;
    const mat4 &GetInverseProjection() const noexcept;

  protected:
    mat4 m_Projection{1.f};
    mat4 m_InverseProjection{1.f};
};

ONYX_DIMENSION_TEMPLATE class ONYX_API Camera : public ICamera
{
  public:
    Camera(const Transform<N> &p_Transform = {}) noexcept;
    virtual void SetAspectRatio(f32 p_Aspect) noexcept;

    vec<N> ScreenToWorld(const vec2 &p_Screen) const noexcept;
    vec2 WorldToScreen(const vec<N> &p_World) const noexcept;

    ONYX::Transform<N> Transform;
};

using Camera2D = Camera<2>;
using Camera3D = Camera<3>;

} // namespace ONYX