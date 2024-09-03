#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
class Orthographic2D : public Camera2D
{
  public:
    Orthographic2D() noexcept = default;
    Orthographic2D(f32 p_Aspect, f32 p_Size, f32 p_Rotation = 0.f);
    Orthographic2D(const vec2 &p_Position, f32 p_Aspect, f32 p_Size, f32 p_Rotation = 0.f);
    Orthographic2D(const vec2 &p_Size, f32 p_Rotation = 0.f);
    Orthographic2D(const vec2 &p_Position, const vec2 &p_Size, f32 p_Rotation = 0.f);

    void UpdateMatrices() noexcept override;

    f32 Size() const noexcept;
    void Size(f32 p_Size) noexcept;
};

class Orthographic3D final : public Camera3D
{
  public:
    Orthographic3D() noexcept = default;
    Orthographic3D(f32 p_Aspect, f32 p_XYSize, f32 p_Depth, const mat3 &p_Rotation = mat3(1.f)) noexcept;
    Orthographic3D(const vec3 &p_Position, f32 p_Aspect, f32 p_XYSize, f32 p_Depth,
                   const mat3 &p_Rotation = mat3(1.f)) noexcept;

    Orthographic3D(const vec3 &p_Size, const mat3 &p_Rotation = mat3(1.f)) noexcept;
    Orthographic3D(const vec3 &p_Position, const vec3 &p_Size, const mat3 &p_Rotation = mat3(1.f)) noexcept;

    void UpdateMatrices() noexcept override;

    f32 Size() const;
    void Size(f32 p_Size);
};

} // namespace ONYX