#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct ONYX_API Transform
{
    static mat<N> ComputeTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                   const rot<N> &p_Rotation) noexcept;
    static mat<N> ComputeReverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                          const rot<N> &p_Rotation) noexcept;
    static mat<N> ComputeInverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                          const rot<N> &p_Rotation) noexcept;

    mat<N> ComputeTransform() const noexcept;
    mat<N> ComputeReverseTransform() const noexcept;
    mat<N> ComputeInverseTransform() const noexcept;

    static mat<N> ComputeTranslationScaleMatrix(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    static mat<N> ComputeReverseTranslationScaleMatrix(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    static mat<N> ComputeInverseTranslationScaleMatrix(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;

    mat<N> ComputeTranslationScaleMatrix() const noexcept;
    mat<N> ComputeReverseTranslationScaleMatrix() const noexcept;
    mat<N> ComputeInverseTranslationScaleMatrix() const noexcept;

    static mat<N> ComputeTranslationMatrix(const vec<N> &p_Translation) noexcept;
    static mat<N> ComputeScaleMatrix(const vec<N> &p_Scale) noexcept;
    static mat<N> ComputeRotationMatrix(const rot<N> &p_Rotation) noexcept;

    mat<N> ComputeTranslationMatrix() const noexcept;
    mat<N> ComputeScaleMatrix() const noexcept;
    mat<N> ComputeRotationMatrix() const noexcept;

    static void Extract(const mat<N> &p_Transform, vec<N> *p_Translation, vec<N> *p_Scale, rot<N> *p_Rotation) noexcept;
    static Transform Extract(const mat<N> &p_Transform) noexcept;

    static vec<N> ExtractTranslation(const mat<N> &p_Transform) noexcept;
    static vec<N> ExtractScale(const mat<N> &p_Transform) noexcept;
    static rot<N> ExtractRotation(const mat<N> &p_Transform) noexcept;

    vec<N> Translation{0.f};
    vec<N> Scale{1.f};
    rot<N> Rotation = RotType<N>::Identity;
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX