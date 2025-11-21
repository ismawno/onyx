#include "onyx/core/pch.hpp"
#include "onyx/property/transform.hpp"

namespace Onyx
{
void ApplyCoordinateSystemExtrinsic(f32m4 &p_Transform)
{
    // Essentially, a rotation around the x axis
    for (u32 i = 0; i < 4; ++i)
        for (u32 j = 1; j < 3; ++j)
            p_Transform[i][j] = -p_Transform[i][j];
}
void ApplyCoordinateSystemIntrinsic(f32m4 &p_Transform)
{
    // Essentially, a rotation around the x axis
    p_Transform[1] = -p_Transform[1];
    p_Transform[2] = -p_Transform[2];
}

template <Dimension D>
f32m<D> ITransform<D>::ComputeTransform(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation)
{
    if constexpr (D == D2)
    {
        const f32m2 rmat = ComputeRotationMatrix(p_Rotation);
        return f32m3{f32v3(rmat[0] * p_Scale[0], 0.f), f32v3(rmat[1] * p_Scale[1], 0.f), f32v3{p_Translation, 1.f}};
    }
    else
    {
        const f32m3 rmat = ComputeRotationMatrix(p_Rotation);
        return f32m4{f32v4(rmat[0] * p_Scale[0], 0.f), f32v4(rmat[1] * p_Scale[1], 0.f),
                     f32v4(rmat[2] * p_Scale[2], 0.f), f32v4{p_Translation, 1.f}};
    }
}

template <Dimension D>
f32m<D> ITransform<D>::ComputeReversedTransform(const f32v<D> &p_Translation, const f32v<D> &p_Scale,
                                                const rot<D> &p_Rotation)
{
    if constexpr (D == D2)
    {
        f32m2 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        const f32v2 translation = rmat * p_Translation;

        return f32m3{f32v3(rmat[0], 0.f), f32v3(rmat[1], 0.f), f32v3{translation, 1.f}};
    }
    else
    {
        f32m3 rmat = ComputeRotationMatrix(p_Rotation);
        rmat[0] *= p_Scale;
        rmat[1] *= p_Scale;
        rmat[2] *= p_Scale;
        const f32v3 translation = rmat * p_Translation;

        return f32m4{f32v4(rmat[0], 0.f), f32v4(rmat[1], 0.f), f32v4(rmat[2], 0.f), f32v4{translation, 1.f}};
    }
}
template <Dimension D>
f32m<D> ITransform<D>::ComputeInverseTransform(const f32v<D> &p_Translation, const f32v<D> &p_Scale,
                                               const rot<D> &p_Rotation)
{
    if constexpr (D == D2)
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeReversedTransform(-p_Translation, 1.f / p_Scale, Math::Conjugate(p_Rotation));
}
template <Dimension D>
f32m<D> ITransform<D>::ComputeInverseReversedTransform(const f32v<D> &p_Translation, const f32v<D> &p_Scale,
                                                       const rot<D> &p_Rotation)
{
    if constexpr (D == D2)
        return ComputeTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeTransform(-p_Translation, 1.f / p_Scale, Math::Conjugate(p_Rotation));
}

template <Dimension D> f32m<D> ITransform<D>::ComputeTransform() const
{
    return ComputeTransform(Translation, Scale, Rotation);
}
template <Dimension D> f32m<D> ITransform<D>::ComputeReversedTransform() const
{
    return ComputeReversedTransform(Translation, Scale, Rotation);
}
template <Dimension D> f32m<D> ITransform<D>::ComputeInverseTransform() const
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}
template <Dimension D> f32m<D> ITransform<D>::ComputeInverseReversedTransform() const
{
    return ComputeInverseReversedTransform(Translation, Scale, Rotation);
}

template <Dimension D>
void ITransform<D>::TranslateIntrinsic(f32m<D> &p_Transform, const u32 p_Axis, const f32 p_Translation)
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Transform[p_Axis][i] * p_Translation;
}
template <Dimension D> void ITransform<D>::TranslateIntrinsic(f32m<D> &p_Transform, const f32v<D> &p_Translation)
{
    for (u32 i = 0; i < D; ++i)
        TranslateIntrinsic(p_Transform, i, p_Translation[i]);
}

template <Dimension D>
void ITransform<D>::TranslateExtrinsic(f32m<D> &p_Transform, const u32 p_Axis, const f32 p_Translation)
{
    p_Transform[D][p_Axis] += p_Translation;
}
template <Dimension D> void ITransform<D>::TranslateExtrinsic(f32m<D> &p_Transform, const f32v<D> &p_Translation)
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[D][i] += p_Translation[i];
}

template <Dimension D> void ITransform<D>::ScaleIntrinsic(f32m<D> &p_Transform, const u32 p_Axis, const f32 p_Scale)
{
    for (u32 i = 0; i < D; ++i)
        p_Transform[p_Axis][i] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleIntrinsic(f32m<D> &p_Transform, const f32v<D> &p_Scale)
{
    for (u32 i = 0; i < D; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[i];
}

template <Dimension D> void ITransform<D>::ScaleExtrinsic(f32m<D> &p_Transform, const u32 p_Axis, const f32 p_Scale)
{
    for (u32 i = 0; i < D + 1; ++i)
        p_Transform[i][p_Axis] *= p_Scale;
}
template <Dimension D> void ITransform<D>::ScaleExtrinsic(f32m<D> &p_Transform, const f32v<D> &p_Scale)
{
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 0; j < D; ++j)
            p_Transform[i][j] *= p_Scale[j];
}

void Transform<D2>::RotateIntrinsic(f32m3 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = ComputeRotationMatrix(p_Angle);
    const f32m2 submat = f32m2{p_Transform} * rot;
    p_Transform[0] = f32v3{submat[0], 0.f};
    p_Transform[1] = f32v3{submat[1], 0.f};
}
void Transform<D2>::RotateExtrinsic(f32m3 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = ComputeRotationMatrix(p_Angle);
    const f32m3x2 submat = rot * f32m3x2{p_Transform};
    p_Transform = f32m3{f32v3{submat[0], 0.f}, f32v3{submat[1], 0.f}, f32v3{submat[2], 1.f}};
}

void Transform<D3>::RotateXIntrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const f32m2x3 submat = f32m2x3{f32v3{p_Transform[1]}, f32v3{p_Transform[2]}} * rot;
    p_Transform[1] = f32v4{submat[0], 0.f};
    p_Transform[2] = f32v4{submat[1], 0.f};
}
void Transform<D3>::RotateYIntrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const f32m2x3 submat = f32m2x3{f32v3{p_Transform[0]}, f32v3{p_Transform[2]}} * rot;
    p_Transform[0] = f32v4{submat[0], 0.f};
    p_Transform[2] = f32v4{submat[1], 0.f};
}
void Transform<D3>::RotateZIntrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const f32m2x3 submat = f32m2x3{f32v3{p_Transform[0]}, f32v3{p_Transform[1]}} * rot;
    p_Transform[0] = f32v4{submat[0], 0.f};
    p_Transform[1] = f32v4{submat[1], 0.f};
}

void Transform<D3>::RotateXExtrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const f32m4x2 submat =
        rot * f32m4x2{f32v2{p_Transform[0][1], p_Transform[0][2]}, f32v2{p_Transform[1][1], p_Transform[1][2]},
                      f32v2{p_Transform[2][1], p_Transform[2][2]}, f32v2{p_Transform[3][1], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][1] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateYExtrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(-p_Angle);
    const f32m4x2 submat =
        rot * f32m4x2{f32v2{p_Transform[0][0], p_Transform[0][2]}, f32v2{p_Transform[1][0], p_Transform[1][2]},
                      f32v2{p_Transform[2][0], p_Transform[2][2]}, f32v2{p_Transform[3][0], p_Transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateZExtrinsic(f32m4 &p_Transform, const f32 p_Angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(p_Angle);
    const f32m4x2 submat = rot * f32m4x2{p_Transform};
    for (u32 i = 0; i < 4; ++i)
    {
        p_Transform[i][0] = submat[i][0];
        p_Transform[i][1] = submat[i][1];
    }
}

void Transform<D3>::RotateIntrinsic(f32m4 &p_Transform, const f32q &p_Quaternion)
{
    const f32m3 rot = ComputeRotationMatrix(p_Quaternion);
    const f32m3 submat = f32m3{p_Transform} * rot;
    p_Transform[0] = f32v4{submat[0], 0.f};
    p_Transform[1] = f32v4{submat[1], 0.f};
    p_Transform[2] = f32v4{submat[2], 0.f};
}
void Transform<D3>::RotateExtrinsic(f32m4 &p_Transform, const f32q &p_Quaternion)
{
    const f32m3 rot = ComputeRotationMatrix(p_Quaternion);
    const f32m4x3 submat = rot * f32m4x3{p_Transform};
    p_Transform = f32m4{f32v4{submat[0], 0.f}, f32v4{submat[1], 0.f}, f32v4{submat[2], 0.f}, f32v4{submat[3], 1.f}};
}

template <Dimension D>
void ITransform<D>::Extract(const f32m<D> &p_Transform, f32v<D> *p_Translation, f32v<D> *p_Scale, rot<D> *p_Rotation)
{
    *p_Translation = ExtractTranslation(p_Transform);
    *p_Scale = ExtractScale(p_Transform);
    *p_Rotation = ExtractRotation(p_Transform);
}

Transform<D2> Transform<D2>::Extract(const f32m3 &p_Transform)
{
    Transform<D2> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}
Transform<D3> Transform<D3>::Extract(const f32m4 &p_Transform)
{
    Transform<D3> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

Transform<D3> Transform<D2>::Promote(const Transform &p_Transform)
{
    Transform<D3> transform;
    transform.Translation = f32v3{p_Transform.Translation, 0.f};
    transform.Scale = f32v3{p_Transform.Scale, 1.f};
    transform.Rotation = f32q{f32v3{0.f, 0.f, p_Transform.Rotation}};
    return transform;
}
Transform<D3> Transform<D2>::Promote()
{
    Transform<D3> transform;
    transform.Translation = f32v3{Translation, 0.f};
    transform.Scale = f32v3{Scale, 1.f};
    transform.Rotation = f32q{f32v3{0.f, 0.f, Rotation}};
    return transform;
}
f32m4 Transform<D2>::Promote(const f32m3 &p_Transform)
{
    f32m4 t4 = f32m4::Identity();
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

template <Dimension D> f32v<D> ITransform<D>::ExtractTranslation(const f32m<D> &p_Transform)
{
    return f32v<D>{p_Transform[D]};
}
template <Dimension D> f32v<D> ITransform<D>::ExtractScale(const f32m<D> &p_Transform)
{
    if constexpr (D == D2)
        return f32v2{Math::Norm(f32v2{p_Transform[0]}), Math::Norm(f32v2{p_Transform[1]})};
    else
        return f32v3{Math::Norm(f32v3{p_Transform[0]}), Math::Norm(f32v3{p_Transform[1]}),
                     Math::Norm(f32v3{p_Transform[2]})};
}
template <Dimension D> rot<D> ITransform<D>::ExtractRotation(const f32m<D> &p_Transform)
{
    if constexpr (D == D2)
        return Math::AntiTangent(p_Transform[0][1], p_Transform[0][0]);
    else
    {
        f32v3 angles;
        angles[0] = Math::AntiTangent(p_Transform[1][2], p_Transform[2][2]);
        angles[1] = Math::AntiTangent(-p_Transform[0][2], Math::SquareRoot(p_Transform[1][2] * p_Transform[1][2] +
                                                                           p_Transform[2][2] * p_Transform[2][2]));
        angles[2] = Math::AntiTangent(p_Transform[0][1], p_Transform[0][0]);
        return f32q{angles};
    }
}

template struct ONYX_API ITransform<D2>;
template struct ONYX_API ITransform<D3>;

} // namespace Onyx
