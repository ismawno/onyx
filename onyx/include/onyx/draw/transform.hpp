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

    void RotateLocal(const quat &p_Rotation) noexcept;
    void RotateLocal(const vec3 &p_Angles) noexcept;

    void RotateLocalX(f32 p_Angle) noexcept;
    void RotateLocalY(f32 p_Angle) noexcept;
    void RotateLocalZ(f32 p_Angle) noexcept;

    void RotateGlobal(const quat &p_Rotation) noexcept;
    void RotateGlobal(const vec3 &p_Angles) noexcept;

    void RotateGlobalX(f32 p_Angle) noexcept;
    void RotateGlobalY(f32 p_Angle) noexcept;
    void RotateGlobalZ(f32 p_Angle) noexcept;

    vec3 Position{0.f};
    vec3 Scale{1.f};
    vec3 Origin{0.f};

    // Up to the user to keep this normalized if it modifies it directly!
    quat Rotation{1.f, 0.f, 0.f, 0.f};
    Transform *Parent{nullptr};
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX