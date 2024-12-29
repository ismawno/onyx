#include "onyx/core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "tkit/core/logging.hpp"

namespace Onyx
{
template <Dimension D>
fmat<D> ITransform<D>::ComputeTransform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                        const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
    {
        const fmat2 rmat = ComputeRotationMatrix(p_Rotation);
        return fmat3{fvec3(rmat[0] * p_Scale.x, 0.f), fvec3(rmat[1] * p_Scale.y, 0.f), fvec3{p_Translation, 1.f}};
    }
    else
    {
        const fmat3 rmat = ComputeRotationMatrix(p_Rotation);
        return fmat4{fvec4(rmat[0] * p_Scale.x, 0.f), fvec4(rmat[1] * p_Scale.y, 0.f), fvec4(rmat[2] * p_Scale.z, 0.f),
                     fvec4{p_Translation, 1.f}};
    }
}

template <Dimension D>
fmat<D> ITransform<D>::ComputeReversedTransform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                                const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
    {
        fmat2 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        const fvec2 translation = rmat * p_Translation;

        return fmat3{fvec3(rmat[0], 0.f), fvec3(rmat[1], 0.f), fvec3{translation, 1.f}};
    }
    else
    {
        fmat3 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        rmat[2] *= p_Scale;
        const fvec3 translation = rmat * p_Translation;

        return fmat4{fvec4(rmat[0], 0.f), fvec4(rmat[1], 0.f), fvec4(rmat[2], 0.f), fvec4{translation, 1.f}};
    }
}
template <Dimension D>
fmat<D> ITransform<D>::ComputeInverseTransform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                               const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}
template <Dimension D>
fmat<D> ITransform<D>::ComputeInverseReversedTransform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                                       const rot<D> &p_Rotation) noexcept
{
    if constexpr (D == D2)
        return ComputeTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}

template <Dimension D> fmat<D> ITransform<D>::ComputeTransform() const noexcept
{
    return ComputeTransform(Translation, Scale, Rotation);
}
template <Dimension D> fmat<D> ITransform<D>::ComputeReversedTransform() const noexcept
{
    return ComputeReversedTransform(Translation, Scale, Rotation);
}
template <Dimension D> fmat<D> ITransform<D>::ComputeInverseTransform() const noexcept
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}
template <Dimension D> fmat<D> ITransform<D>::ComputeInverseReversedTransform() const noexcept
{
    return ComputeInverseReversedTransform(Translation, Scale, Rotation);
}

template <Dimension D>
void ITransform<D>::TranslateIntrinsic(fmat<D> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Transform[p_Axis][i] * p_Translation;
}
template <Dimension D>
void ITransform<D>::TranslateIntrinsic(fmat<D> &p_Transform, const fvec<D> &p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        TranslateIntrinsic(p_Transform, i, p_Translation[i]);
}

template <Dimension D>
void ITransform<D>::TranslateExtrinsic(fmat<D> &p_Transform, const u32 p_Axis, const f32 p_Translation) noexcept
{
    p_Transform[D][p_Axis] += p_Translation;
}
template <Dimension D>
void ITransform<D>::TranslateExtrinsic(fmat<D> &p_Transform, const fvec<D> &p_Translation) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Translation[i];
}

template <Dimension D>
void ITransform<D>::ScaleIntrinsic(fmat<D> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[p_Axis][i] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleIntrinsic(fmat<D> &p_Transform, const fvec<D> &p_Scale) noexcept
{
    for (u32 i = 0; i < D; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[i];
}

template <Dimension D>
void ITransform<D>::ScaleExtrinsic(fmat<D> &p_Transform, const u32 p_Axis, const f32 p_Scale) noexcept
{
    for (u32 i = 0; i < D + 1; ++i)
        p_Transform[i][p_Axis] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleExtrinsic(fmat<D> &p_Transform, const fvec<D> &p_Scale) noexcept
{
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[j];
}

using fmat3x2 = glm::mat<D3, 2, f32>;
using fmat4x2 = glm::mat<4, 2, f32>;
using fmat2x3 = glm::mat<D2, 3, f32>;
using fmat4x3 = glm::mat<4, 3, f32>;

void Transform<D2>::RotateIntrinsic(fmat3 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = ComputeRotationMatrix(p_Angle);
    const fmat2 submat = fmat2{p_Transform} * rot;
    p_Transform[0] = fvec3{submat[0], 0.f};
    p_Transform[1] = fvec3{submat[1], 0.f};
}
void Transform<D2>::RotateExtrinsic(fmat3 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = ComputeRotationMatrix(p_Angle);
    const fmat3x2 submat = rot * fmat3x2{p_Transform};
    p_Transform = fmat3{fvec3{submat[0], 0.f}, fvec3{submat[1], 0.f}, fvec3{submat[2], 1.f}};
}

void Transform<D3>::RotateXIntrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const fmat2x3 submat = fmat2x3{fvec3{p_Transform[1]}, fvec3{p_Transform[2]}} * rot;
    p_Transform[1] = fvec4{submat[0], 0.f};
    p_Transform[2] = fvec4{submat[1], 0.f};
}
void Transform<D3>::RotateYIntrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const fmat2x3 submat = fmat2x3{fvec3{p_Transform[0]}, fvec3{p_Transform[2]}} * rot;
    p_Transform[0] = fvec4{submat[0], 0.f};
    p_Transform[2] = fvec4{submat[1], 0.f};
}
void Transform<D3>::RotateZIntrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const fmat2x3 submat = fmat2x3{fvec3{p_Transform[0]}, fvec3{p_Transform[1]}} * rot;
    p_Transform[0] = fvec4{submat[0], 0.f};
    p_Transform[1] = fvec4{submat[1], 0.f};
}

void Transform<D3>::RotateXExtrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const fmat4x2 submat = rot * fmat4x2{{p_Transform[0][1], p_Transform[0][2]},
                                         {p_Transform[1][1], p_Transform[1][2]},
                                         {p_Transform[2][1], p_Transform[2][2]},
                                         {p_Transform[3][1], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][1] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateYExtrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const fmat4x2 submat = rot * fmat4x2{{p_Transform[0][0], p_Transform[0][2]},
                                         {p_Transform[1][0], p_Transform[1][2]},
                                         {p_Transform[2][0], p_Transform[2][2]},
                                         {p_Transform[3][0], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateZExtrinsic(fmat4 &p_Transform, const f32 p_Angle) noexcept
{
    const fmat2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const fmat4x2 submat = rot * fmat4x2{p_Transform};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][1] = submat[i][1];
    }
}

void Transform<D3>::RotateIntrinsic(fmat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const fmat3 rot = ComputeRotationMatrix(p_Quaternion);
    const fmat3 submat = fmat3{p_Transform} * rot;
    p_Transform[0] = fvec4{submat[0], 0.f};
    p_Transform[1] = fvec4{submat[1], 0.f};
    p_Transform[2] = fvec4{submat[2], 0.f};
}
void Transform<D3>::RotateExtrinsic(fmat4 &p_Transform, const quat &p_Quaternion) noexcept
{
    const fmat3 rot = ComputeRotationMatrix(p_Quaternion);
    const fmat4x3 submat = rot * fmat4x3{p_Transform};
    p_Transform = fmat4{fvec4{submat[0], 0.f}, fvec4{submat[1], 0.f}, fvec4{submat[2], 0.f}, fvec4{submat[3], 1.f}};
}

template <Dimension D>
void ITransform<D>::Extract(const fmat<D> &p_Transform, fvec<D> *p_Translation, fvec<D> *p_Scale,
                            rot<D> *p_Rotation) noexcept
{
    *p_Translation = ExtractTranslation(p_Transform);
    *p_Scale = ExtractScale(p_Transform);
    *p_Rotation = ExtractRotation(p_Transform);
}

Transform<D2> Transform<D2>::Extract(const fmat3 &p_Transform) noexcept
{
    Transform<D2> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}
Transform<D3> Transform<D3>::Extract(const fmat4 &p_Transform) noexcept
{
    Transform<D3> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

template <Dimension D> fvec<D> ITransform<D>::ExtractTranslation(const fmat<D> &p_Transform) noexcept
{
    return fvec<D>{p_Transform[D]};
}
template <Dimension D> fvec<D> ITransform<D>::ExtractScale(const fmat<D> &p_Transform) noexcept
{
    if constexpr (D == D2)
        return fvec2{glm::length(fvec2{p_Transform[0]}), glm::length(fvec2{p_Transform[1]})};
    else
        return fvec3{glm::length(fvec3{p_Transform[0]}), glm::length(fvec3{p_Transform[1]}),
                     glm::length(fvec3{p_Transform[2]})};
}
template <Dimension D> rot<D> ITransform<D>::ExtractRotation(const fmat<D> &p_Transform) noexcept
{
    if constexpr (D == D2)
        return glm::atan(p_Transform[0][1], p_Transform[0][0]);
    else
    {
        fvec3 angles;
        angles.x = glm::atan(p_Transform[1][2], p_Transform[2][2]);
        angles.y = glm::atan(-p_Transform[0][2],
                             glm::sqrt(p_Transform[1][2] * p_Transform[1][2] + p_Transform[2][2] * p_Transform[2][2]));
        angles.z = glm::atan(p_Transform[0][1], p_Transform[0][0]);
        return quat{angles};
    }
}

template struct ITransform<D2>;
template struct ITransform<D3>;

} // namespace Onyx