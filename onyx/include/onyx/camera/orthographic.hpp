#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Orthographic;

template <> class ONYX_API Orthographic<2> final : public Camera<2>
{
  public:
    Orthographic() noexcept = default;
    Orthographic(f32 p_Aspect, f32 p_Size, f32 p_Rotation = 0.f);
    Orthographic(const vec2 &p_Position, f32 p_Aspect, f32 p_Size, f32 p_Rotation = 0.f);
    Orthographic(const vec2 &p_Size, f32 p_Rotation = 0.f);
    Orthographic(const vec2 &p_Position, const vec2 &p_Size, f32 p_Rotation = 0.f);

    void UpdateMatrices() noexcept override;

    f32 Size() const noexcept;
    void Size(f32 p_Size) noexcept;
};

template <> class ONYX_API Orthographic<3> final : public Camera<3>
{
  public:
    Orthographic() noexcept = default;
    Orthographic(f32 p_Aspect, f32 p_XYSize, f32 p_Depth, const mat3 &p_Rotation = mat3(1.f)) noexcept;
    Orthographic(const vec3 &p_Position, f32 p_Aspect, f32 p_XYSize, f32 p_Depth,
                 const mat3 &p_Rotation = mat3(1.f)) noexcept;

    Orthographic(const vec3 &p_Size, const mat3 &p_Rotation = mat3(1.f)) noexcept;
    Orthographic(const vec3 &p_Position, const vec3 &p_Size, const mat3 &p_Rotation = mat3(1.f)) noexcept;

    void UpdateMatrices() noexcept override;

    f32 Size() const;
    void Size(f32 p_Size);
};

} // namespace ONYX