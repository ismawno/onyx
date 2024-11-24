#include "onyx/core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/core/logging.hpp"

namespace ONYX
{
template <Dimension D>
mat<D> ITransform<D>::ComputeTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                       const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
    {
        const mat2 rmat = ComputeRotationMatrix(p_Rotation);
        return mat3{vec3(rmat[0] * p_Scale.x, 0.f), vec3(rmat[1] * p_Scale.y, 0.f), vec3{p_Translation, 1.f}};
    }
    else
    {
        const mat3 rmat = ComputeRotationMatrix(p_Rotation);
        return mat4{vec4(rmat[0] * p_Scale.x, 0.f), vec4(rmat[1] * p_Scale.y, 0.f), vec4(rmat[2] * p_Scale.z, 0.f),
                    vec4{p_Translation, 1.f}};
    }
}

template <Dimension D>
mat<D> ITransform<D>::ComputeReversedTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                               const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
    {
        mat2 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        const vec2 translation = rmat * p_Translation;

        return mat3{vec3(rmat[0], 0.f), vec3(rmat[1], 0.f), vec3{translation, 1.f}};
    }
    else
    {
        mat3 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        rmat[2] *= p_Scale;
        const vec3 translation = rmat * p_Translation;

        return mat4{vec4(rmat[0], 0.f), vec4(rmat[1], 0.f), vec4(rmat[2], 0.f), vec4{translation, 1.f}};
    }
}
template <Dimension D>
mat<D> ITransform<D>::ComputeInverseTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                              const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}
template <Dimension D>
mat<D> ITransform<D>::ComputeInverseReversedTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                                      const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
        return ComputeTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}

template <Dimension D> mat<D> ITransform<D>::ComputeTransform() const noexcept
{
    return ComputeTransform(Translation, Scale, Rotation);
}
template <Dimension D> mat<D> ITransform<D>::ComputeReversedTransform() const noexcept
{
    return ComputeReversedTransform(Translation, Scale, Rotation);
}
template <Dimension D> mat<D> ITransform<D>::ComputeInverseTransform() const noexcept
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}
template <Dimension D> mat<D> ITransform<D>::ComputeInverseReversedTransform() const noexcept
{
    return ComputeInverseReversedTransform(Translation, Scale, Rotation);
}

template <Dimension D>
void ITransform<D>::TranslateIntrinsic(mat<D> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Transform[p_Axis][i] * p_Translation;
}
template <Dimension D> void ITransform<D>::TranslateIntrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        TranslateIntrinsic(p_Transform, i, p_Translation[i]);
}

template <Dimension D>
void ITransform<D>::TranslateExtrinsic(mat<D> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    p_Transform[D][p_Axis] += p_Translation;
}
template <Dimension D> void ITransform<D>::TranslateExtrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Translation[i];
}

template <Dimension D>
void ITransform<D>::ScaleIntrinsic(mat<D> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[p_Axis][i] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleIntrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept
{
    for (u32 i = 0; i < D; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[i];
}

template <Dimension D>
void ITransform<D>::ScaleExtrinsic(mat<D> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < D + 1; ++i)
        p_Transform[i][p_Axis] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleExtrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept
{
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[j];
}

using mat3x2 = glm::mat<D3, 2, f32>;
using mat4x2 = glm::mat<4, 2, f32>;
using mat2x3 = glm::mat<D2, 3, f32>;
using mat4x3 = glm::mat<4, 3, f32>;

void Transform<D2>::RotateIntrinsic(mat3 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = ComputeRotationMatrix(p_Angle);
    const mat2 submat = mat2{p_Transform} * rot;
    p_Transform[0] = vec3{submat[0], 0.f};
    p_Transform[1] = vec3{submat[1], 0.f};
}
void Transform<D2>::RotateExtrinsic(mat3 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = ComputeRotationMatrix(p_Angle);
    const mat3x2 submat = rot * mat3x2{p_Transform};
    p_Transform = mat3{vec3{submat[0], 0.f}, vec3{submat[1], 0.f}, vec3{submat[2], 1.f}};
}

void Transform<D3>::RotateXIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[1]}, vec3{p_Transform[2]}} * rot;
    p_Transform[1] = vec4{submat[0], 0.f};
    p_Transform[2] = vec4{submat[1], 0.f};
}
void Transform<D3>::RotateYIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[0]}, vec3{p_Transform[2]}} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[2] = vec4{submat[1], 0.f};
}
void Transform<D3>::RotateZIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[0]}, vec3{p_Transform[1]}} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[1] = vec4{submat[1], 0.f};
}

void Transform<D3>::RotateXExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const mat4x2 submat = rot * mat4x2{{p_Transform[0][1], p_Transform[0][2]},
                                       {p_Transform[1][1], p_Transform[1][2]},
                                       {p_Transform[2][1], p_Transform[2][2]},
                                       {p_Transform[3][1], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][1] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateYExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const mat4x2 submat = rot * mat4x2{{p_Transform[0][0], p_Transform[0][2]},
                                       {p_Transform[1][0], p_Transform[1][2]},
                                       {p_Transform[2][0], p_Transform[2][2]},
                                       {p_Transform[3][0], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateZExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const mat4x2 submat = rot * mat4x2{p_Transform};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][1] = submat[i][1];
    }
}

void Transform<D3>::RotateIntrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const mat3 rot = ComputeRotationMatrix(p_Quaternion);
    const mat3 submat = mat3{p_Transform} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[1] = vec4{submat[1], 0.f};
    p_Transform[2] = vec4{submat[2], 0.f};
}
void Transform<D3>::RotateExtrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const mat3 rot = ComputeRotationMatrix(p_Quaternion);
    const mat4x3 submat = rot * mat4x3{p_Transform};
    p_Transform = mat4{vec4{submat[0], 0.f}, vec4{submat[1], 0.f}, vec4{submat[2], 0.f}, vec4{submat[3], 1.f}};
}

template <Dimension D>
void ITransform<D>::Extract(const mat<D> &p_Transform, vec<D> *p_Translation, vec<D> *p_Scale,
                            rot<D> *p_Rotation) noexcept
{
    *p_Translation = ExtractTranslation(p_Transform);
    *p_Scale = ExtractScale(p_Transform);
    *p_Rotation = ExtractRotation(p_Transform);
}

Transform<D2> Transform<D2>::Extract(const mat3 &p_Transform) noexcept
{
    Transform<D2> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}
Transform<D3> Transform<D3>::Extract(const mat4 &p_Transform) noexcept
{
    Transform<D3> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

template <Dimension D> vec<D> ITransform<D>::ExtractTranslation(const mat<D> &p_Transform) noexcept
{
    return vec<D>{p_Transform[D]};
}
template <Dimension D> vec<D> ITransform<D>::ExtractScale(const mat<D> &p_Transform) noexcept
{
    if constexpr (D == D2)
        return vec2{glm::length(vec2{p_Transform[0]}), glm::length(vec2{p_Transform[1]})};
    else
        return vec3{glm::length(vec3{p_Transform[0]}), glm::length(vec3{p_Transform[1]}),
                    glm::length(vec3{p_Transform[2]})};
}
template <Dimension D> rot<D> ITransform<D>::ExtractRotation(const mat<D> &p_Transform) noexcept
{
    if constexpr (D == D2)
        return glm::atan(p_Transform[0][1], p_Transform[0][0]);
    else
    {
        vec3 angles;
        angles.x = glm::atan(p_Transform[1][2], p_Transform[2][2]);
        angles.y = glm::atan(-p_Transform[0][2],
                             glm::sqrt(p_Transform[1][2] * p_Transform[1][2] + p_Transform[2][2] * p_Transform[2][2]));
        angles.z = glm::atan(p_Transform[0][1], p_Transform[0][0]);
        return quat{angles};
    }
}

template struct ITransform<D2>;
template struct ITransform<D3>;

} // namespace ONYX