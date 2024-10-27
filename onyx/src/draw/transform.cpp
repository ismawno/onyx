#include "core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/core/logging.hpp"

namespace ONYX
{
static mat2 rotationMatrix2(const f32 p_Angle) noexcept
{
    const f32 c = glm::cos(p_Angle);
    const f32 s = glm::sin(p_Angle);
    return mat2{c, s, -s, c};
}
static mat3 rotationMatrix3(const quat &p_Quaternion) noexcept
{
    return glm::toMat3(p_Quaternion);
}

template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                       const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
        const mat2 rotScale = rotationMatrix2(p_Rotation) * scale;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), vec3{p_Translation, 1.f}};
    }
    else
    {
        const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
        const mat3 rotScale = rotationMatrix3(p_Rotation) * scale;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4{p_Translation, 1.f}};
    }
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeAxesTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                           const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
        const mat2 rotScale = scale * rotationMatrix2(p_Rotation);
        const vec2 translation = rotScale * p_Translation;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), vec3{translation, 1.f}};
    }
    else
    {
        const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
        const mat3 rotScale = scale * rotationMatrix3(p_Rotation);
        const vec3 translation = rotScale * p_Translation;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4{translation, 1.f}};
    }
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeInverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                              const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
        return ComputeAxesTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeAxesTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeInverseAxesTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                  const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
        return ComputeTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}

template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeTransform() const noexcept
{
    return ComputeTransform(Translation, Scale, Rotation);
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeAxesTransform() const noexcept
{
    return ComputeAxesTransform(Translation, Scale, Rotation);
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeInverseTransform() const noexcept
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}
template <u32 N>
    requires(IsDim<N>())
mat<N> ITransform<N>::ComputeInverseAxesTransform() const noexcept
{
    return ComputeInverseAxesTransform(Translation, Scale, Rotation);
}

template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::TranslateIntrinsic(mat<N> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    for (u32 i = 0; i < N; ++i)
        p_Transform[N][i] += p_Transform[p_Axis][i] * p_Translation;
}
template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::TranslateIntrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept
{
    for (u32 i = 0; i < N; ++i)
        TranslateIntrinsic(p_Transform, i, p_Translation[i]);
}

template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::TranslateExtrinsic(mat<N> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    p_Transform[N][p_Axis] += p_Translation;
}
template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::TranslateExtrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept
{
    for (u32 i = 0; i < N; ++i)
        p_Transform[N][i] += p_Translation[i];
}

template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::ScaleIntrinsic(mat<N> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < N; ++i)
        p_Transform[p_Axis][i] *= p_Scale;
}
template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::ScaleIntrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept
{
    for (u32 i = 0; i < N; ++i)
        for (u32 j = 0; j < N; ++j)
            p_Transform[i][j] *= p_Scale[i];
}

template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::ScaleExtrinsic(mat<N> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < N + 1; ++i)
        p_Transform[i][p_Axis] *= p_Scale;
}
template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::ScaleExtrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept
{
    for (u32 i = 0; i < N + 1; ++i)
        for (u32 j = 0; j < N; ++j)
            p_Transform[i][j] *= p_Scale[j];
}

using mat3x2 = glm::mat<3, 2, f32>;
using mat4x2 = glm::mat<4, 2, f32>;
using mat2x3 = glm::mat<2, 3, f32>;
using mat4x3 = glm::mat<4, 3, f32>;

void Transform<2>::RotateIntrinsic(mat3 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
    const mat2 submat = mat2{p_Transform} * rot;
    p_Transform[0] = vec3{submat[0], 0.f};
    p_Transform[1] = vec3{submat[1], 0.f};
}
void Transform<2>::RotateExtrinsic(mat3 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
    const mat3x2 submat = rot * mat3x2{p_Transform};
    p_Transform = mat3{vec3{submat[0], 0.f}, vec3{submat[1], 0.f}, vec3{submat[2], 1.f}};
}

void Transform<3>::RotateXIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[1]}, vec3{p_Transform[2]}} * rot;
    p_Transform[1] = vec4{submat[0], 0.f};
    p_Transform[2] = vec4{submat[1], 0.f};
}
void Transform<3>::RotateYIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(-p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[0]}, vec3{p_Transform[2]}} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[2] = vec4{submat[1], 0.f};
}
void Transform<3>::RotateZIntrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
    const mat2x3 submat = mat2x3{vec3{p_Transform[0]}, vec3{p_Transform[1]}} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[1] = vec4{submat[1], 0.f};
}

void Transform<3>::RotateXExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
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
void Transform<3>::RotateYExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(-p_Angle);
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
void Transform<3>::RotateZExtrinsic(mat4 &p_Transform, const f32 p_Angle) noexcept
{
    const mat2 rot = rotationMatrix2(p_Angle);
    const mat4x2 submat = rot * mat4x2{p_Transform};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][1] = submat[i][1];
    }
}

void Transform<3>::RotateIntrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const mat3 rot = rotationMatrix3(p_Quaternion);
    const mat3 submat = mat3{p_Transform} * rot;
    p_Transform[0] = vec4{submat[0], 0.f};
    p_Transform[1] = vec4{submat[1], 0.f};
    p_Transform[2] = vec4{submat[2], 0.f};
}
void Transform<3>::RotateExtrinsic(mat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const mat3 rot = rotationMatrix3(p_Quaternion);
    const mat4x3 submat = rot * mat4x3{p_Transform};
    p_Transform = mat4{vec4{submat[0], 0.f}, vec4{submat[1], 0.f}, vec4{submat[2], 0.f}, vec4{submat[3], 1.f}};
}

template <u32 N>
    requires(IsDim<N>())
void ITransform<N>::Extract(const mat<N> &p_Transform, vec<N> *p_Translation, vec<N> *p_Scale,
                            rot<N> *p_Rotation) noexcept
{
    *p_Translation = ExtractTranslation(p_Transform);
    *p_Scale = ExtractScale(p_Transform);
    *p_Rotation = ExtractRotation(p_Transform);
}

Transform<2> Transform<2>::Extract(const mat3 &p_Transform) noexcept
{
    Transform2D transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}
Transform<3> Transform<3>::Extract(const mat4 &p_Transform) noexcept
{
    Transform3D transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

template <u32 N>
    requires(IsDim<N>())
vec<N> ITransform<N>::ExtractTranslation(const mat<N> &p_Transform) noexcept
{
    return vec<N>{p_Transform[N]};
}
template <u32 N>
    requires(IsDim<N>())
vec<N> ITransform<N>::ExtractScale(const mat<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
        return vec2{glm::length(vec2{p_Transform[0]}), glm::length(vec2{p_Transform[1]})};
    else
        return vec3{glm::length(vec3{p_Transform[0]}), glm::length(vec3{p_Transform[1]}),
                    glm::length(vec3{p_Transform[2]})};
}
template <u32 N>
    requires(IsDim<N>())
rot<N> ITransform<N>::ExtractRotation(const mat<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
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

template struct ITransform<2>;
template struct ITransform<3>;

} // namespace ONYX