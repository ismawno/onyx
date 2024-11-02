#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
template <Dimension D> struct ONYX_API ITransform
{
    static mat<D> ComputeTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                   const rot<D> &p_Rotation) noexcept;
    static mat<D> ComputeAxesTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                       const rot<D> &p_Rotation) noexcept;

    static mat<D> ComputeInverseTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                          const rot<D> &p_Rotation) noexcept;
    static mat<D> ComputeInverseAxesTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                              const rot<D> &p_Rotation) noexcept;

    mat<D> ComputeTransform() const noexcept;
    mat<D> ComputeAxesTransform() const noexcept;

    mat<D> ComputeInverseTransform() const noexcept;
    mat<D> ComputeInverseAxesTransform() const noexcept;

    static void TranslateIntrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;
    static void TranslateIntrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept;

    static void TranslateExtrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;
    static void TranslateExtrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept;

    static void ScaleIntrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;
    static void ScaleIntrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept;

    static void ScaleExtrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;
    static void ScaleExtrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept;

    static void Extract(const mat<D> &p_Transform, vec<D> *p_Translation, vec<D> *p_Scale, rot<D> *p_Rotation) noexcept;

    static vec<D> ExtractTranslation(const mat<D> &p_Transform) noexcept;
    static vec<D> ExtractScale(const mat<D> &p_Transform) noexcept;
    static rot<D> ExtractRotation(const mat<D> &p_Transform) noexcept;

    vec<D> Translation{0.f};
    vec<D> Scale{1.f};
    rot<D> Rotation = RotType<D>::Identity;
};

template <Dimension D> struct Transform;

template <> struct ONYX_API Transform<D2> : ITransform<D2>
{
    using ITransform<D2>::Extract;

    static void RotateIntrinsic(mat3 &p_Transform, f32 p_Angle) noexcept;
    static void RotateExtrinsic(mat3 &p_Transform, f32 p_Angle) noexcept;

    static Transform Extract(const mat3 &p_Transform) noexcept;
};

template <> struct ONYX_API Transform<D3> : ITransform<D3>
{
    using ITransform<D3>::Extract;

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

} // namespace ONYX