#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
struct ONYX_API ITransform
{
    static mat<N> ComputeTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                   const rot<N> &p_Rotation) noexcept;
    static mat<N> ComputeAxesTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                       const rot<N> &p_Rotation) noexcept;

    static mat<N> ComputeInverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                          const rot<N> &p_Rotation) noexcept;
    static mat<N> ComputeInverseAxesTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                              const rot<N> &p_Rotation) noexcept;

    mat<N> ComputeTransform() const noexcept;
    mat<N> ComputeAxesTransform() const noexcept;

    mat<N> ComputeInverseTransform() const noexcept;
    mat<N> ComputeInverseAxesTransform() const noexcept;

    static void TranslateIntrinsic(mat<N> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;
    static void TranslateIntrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept;

    static void TranslateExtrinsic(mat<N> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;
    static void TranslateExtrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept;

    static void ScaleIntrinsic(mat<N> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;
    static void ScaleIntrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept;

    static void ScaleExtrinsic(mat<N> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;
    static void ScaleExtrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept;

    static void Extract(const mat<N> &p_Transform, vec<N> *p_Translation, vec<N> *p_Scale, rot<N> *p_Rotation) noexcept;

    static vec<N> ExtractTranslation(const mat<N> &p_Transform) noexcept;
    static vec<N> ExtractScale(const mat<N> &p_Transform) noexcept;
    static rot<N> ExtractRotation(const mat<N> &p_Transform) noexcept;

    vec<N> Translation{0.f};
    vec<N> Scale{1.f};
    rot<N> Rotation = RotType<N>::Identity;
};

template <u32 N>
    requires(IsDim<N>())
struct Transform;

template <> struct ONYX_API Transform<2> : ITransform<2>
{
    using ITransform<2>::Extract;

    static void RotateIntrinsic(mat3 &p_Transform, f32 p_Angle) noexcept;
    static void RotateExtrinsic(mat3 &p_Transform, f32 p_Angle) noexcept;

    static Transform Extract(const mat3 &p_Transform) noexcept;
};

template <> struct ONYX_API Transform<3> : ITransform<3>
{
    using ITransform<3>::Extract;

    static void RotateXIntrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;
    static void RotateYIntrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;
    static void RotateZIntrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;

    static void RotateXExtrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;
    static void RotateYExtrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;
    static void RotateZExtrinsic(mat4 &p_Transform, f32 p_Angle) noexcept;

    static void RotateIntrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept;
    static void RotateExtrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept;

    static Transform Extract(const mat4 &p_Transform) noexcept;
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX