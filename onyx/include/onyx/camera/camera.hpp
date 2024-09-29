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

    virtual void SetAspectRatio(f32 p_Aspect) noexcept = 0;

    virtual mat4 ComputeProjectionView() const noexcept = 0;
    virtual mat4 ComputeInverseProjectionView() const noexcept = 0;

    // This is a very unfortunate virtual method
    virtual bool IsOrthographic() const noexcept = 0;
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