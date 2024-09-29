#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Transform;
ONYX_DIMENSION_TEMPLATE struct ITransform
{
    mat4 ComputeModelTransform() const noexcept;
    mat4 ComputeViewTransform() const noexcept;

    mat4 ComputeInverseModelTransform() const noexcept;
    mat4 ComputeInverseViewTransform() const noexcept;

    void UpdateFromModelTransform(const mat4 &p_ModelTransform) noexcept;

    vec<N> LocalOffset(const vec<N> &p_Offset) const noexcept;
    vec<N> LocalOffsetX(f32 p_Offset) const noexcept;
    vec<N> LocalOffsetY(f32 p_Offset) const noexcept;

    vec<N> Position{0.f};
    vec<N> Scale{1.f};
    vec<N> Origin{0.f};
    rot<N> Rotation = RotType<N>::Identity;
    Transform<N> *Parent = nullptr;
};

template <> struct ONYX_API Transform<2> final : public ITransform<2>
{
    static Transform FromModelTransform(const mat4 &p_ModelTransform) noexcept;
};

template <> struct ONYX_API Transform<3> : public ITransform<3>
{
    vec3 GetEulerAngles() const noexcept;
    void SetEulerAngles(const vec3 &p_Angles) noexcept;

    vec3 LocalOffsetZ(f32 p_Offset) const noexcept;

    void RotateLocalAxis(const quat &p_Rotation) noexcept;
    void RotateLocalAxis(const vec3 &p_Angles) noexcept;
    void RotateLocalAxisX(f32 p_Angle) noexcept;
    void RotateLocalAxisY(f32 p_Angle) noexcept;
    void RotateLocalAxisZ(f32 p_Angle) noexcept;

    void RotateGlobalAxis(const quat &p_Rotation) noexcept;
    void RotateGlobalAxis(const vec3 &p_Angles) noexcept;
    void RotateGlobalAxisX(f32 p_Angle) noexcept;
    void RotateGlobalAxisY(f32 p_Angle) noexcept;
    void RotateGlobalAxisZ(f32 p_Angle) noexcept;

    static Transform FromModelTransform(const mat4 &p_ModelTransform) noexcept;
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX