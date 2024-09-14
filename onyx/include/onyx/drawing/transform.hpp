#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Transform;

template <> struct ONYX_API Transform<2>
{
    mat4 ModelTransform() const noexcept;
    mat4 InverseModelTransform() const noexcept;

    mat4 CameraTransform() const noexcept;
    mat4 InverseCameraTransform() const noexcept;

    vec2 LocalOffset(const vec2 &p_Offset) const noexcept;
    vec2 LocalOffsetX(f32 p_Offset) const noexcept;
    vec2 LocalOffsetY(f32 p_Offset) const noexcept;

    vec2 Position{0.f};
    vec2 Scale{1.f};
    vec2 Origin{0.f};
    f32 Rotation{0.f};
    Transform *Parent{nullptr};
};

template <> struct ONYX_API Transform<3>
{
    mat4 ModelTransform() const noexcept;
    mat4 InverseModelTransform() const noexcept;

    mat4 CameraTransform() const noexcept;
    mat4 InverseCameraTransform() const noexcept;

    vec3 LocalOffset(const vec3 &p_Offset) const noexcept;
    vec3 LocalOffsetX(f32 p_Offset) const noexcept;
    vec3 LocalOffsetY(f32 p_Offset) const noexcept;
    vec3 LocalOffsetZ(f32 p_Offset) const noexcept;

    void RotateLocal(const mat3 &p_Rotation) noexcept;
    void RotateGlobal(const mat3 &p_Rotation) noexcept;

    mat3 InverseRotation() const noexcept;

    static mat3 RotXYZ(const vec3 &p_Rotation) noexcept;
    static mat3 RotXZY(const vec3 &p_Rotation) noexcept;
    static mat3 RotYXZ(const vec3 &p_Rotation) noexcept;
    static mat3 RotYZX(const vec3 &p_Rotation) noexcept;
    static mat3 RotZXY(const vec3 &p_Rotation) noexcept;
    static mat3 RotZYX(const vec3 &p_Rotation) noexcept;

    static mat3 RotXY(f32 p_RotX, f32 p_RotY) noexcept;
    static mat3 RotXZ(f32 p_RotX, f32 p_RotZ) noexcept;
    static mat3 RotYX(f32 p_RotY, f32 p_RotX) noexcept;
    static mat3 RotYZ(f32 p_RotY, f32 p_RotZ) noexcept;
    static mat3 RotZX(f32 p_RotZ, f32 p_RotX) noexcept;
    static mat3 RotZY(f32 p_RotZ, f32 p_RotY) noexcept;

    static mat3 RotX(f32 p_RotX) noexcept;
    static mat3 RotY(f32 p_RotY) noexcept;
    static mat3 RotZ(f32 p_RotZ) noexcept;

    vec3 Position{0.f};
    vec3 Scale{1.f};
    vec3 Origin{0.f};

    // TODO: Change to quaternion/euler angles
    mat3 Rotation{1.f};
    Transform *Parent{nullptr};
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX