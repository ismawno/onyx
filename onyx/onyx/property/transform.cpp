#include "onyx/core/pch.hpp"
#include "onyx/property/transform.hpp"

namespace Onyx
{
void ApplyCoordinateSystemExtrinsic(f32m4 &transform)
{
    // Essentially, a rotation around the x axis
    for (u32 i = 0; i < 4; ++i)
        for (u32 j = 1; j < 3; ++j)
            transform[i][j] = -transform[i][j];
}
void ApplyCoordinateSystemIntrinsic(f32m4 &transform)
{
    // Essentially, a rotation around the x axis
    transform[1] = -transform[1];
    transform[2] = -transform[2];
}

template <Dimension D>
f32m<D> ITransform<D>::ComputeTransform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation)
{
    if constexpr (D == D2)
    {
        const f32m2 rmat = ComputeRotationMatrix(rotation);
        return f32m3{f32v3(rmat[0] * scale[0], 0.f), f32v3(rmat[1] * scale[1], 0.f), f32v3{translation, 1.f}};
    }
    else
    {
        const f32m3 rmat = ComputeRotationMatrix(rotation);
        return f32m4{f32v4(rmat[0] * scale[0], 0.f), f32v4(rmat[1] * scale[1], 0.f), f32v4(rmat[2] * scale[2], 0.f),
                     f32v4{translation, 1.f}};
    }
}

template <Dimension D>
f32m<D> ITransform<D>::ComputeReversedTransform(const f32v<D> &translation, const f32v<D> &scale,
                                                const rot<D> &rotation)
{
    if constexpr (D == D2)
    {
        f32m2 rmat = ComputeRotationMatrix(rotation);
        rmat[0] *= scale;
        rmat[1] *= scale;
        const f32v2 trans = rmat * translation;

        return f32m3{f32v3(rmat[0], 0.f), f32v3(rmat[1], 0.f), f32v3{trans, 1.f}};
    }
    else
    {
        f32m3 rmat = ComputeRotationMatrix(rotation);
        rmat[0] *= scale;
        rmat[1] *= scale;
        rmat[2] *= scale;
        const f32v3 trans = rmat * translation;

        return f32m4{f32v4(rmat[0], 0.f), f32v4(rmat[1], 0.f), f32v4(rmat[2], 0.f), f32v4{trans, 1.f}};
    }
}
template <Dimension D>
f32m<D> ITransform<D>::ComputeInverseTransform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation)
{
    if constexpr (D == D2)
        return ComputeReversedTransform(-translation, 1.f / scale, -rotation);
    else
        return ComputeReversedTransform(-translation, 1.f / scale, Math::Conjugate(rotation));
}
template <Dimension D>
f32m<D> ITransform<D>::ComputeInverseReversedTransform(const f32v<D> &translation, const f32v<D> &scale,
                                                       const rot<D> &rotation)
{
    if constexpr (D == D2)
        return ComputeTransform(-translation, 1.f / scale, -rotation);
    else
        return ComputeTransform(-translation, 1.f / scale, Math::Conjugate(rotation));
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

template <Dimension D> void ITransform<D>::TranslateIntrinsic(f32m<D> &transform, const u32 axis, const f32 translation)
{
    for (u32 i = 0; i < D; ++i)
        transform[D][i] += transform[axis][i] * translation;
}
template <Dimension D> void ITransform<D>::TranslateIntrinsic(f32m<D> &transform, const f32v<D> &translation)
{
    for (u32 i = 0; i < D; ++i)
        TranslateIntrinsic(transform, i, translation[i]);
}

template <Dimension D> void ITransform<D>::TranslateExtrinsic(f32m<D> &transform, const u32 axis, const f32 translation)
{
    transform[D][axis] += translation;
}
template <Dimension D> void ITransform<D>::TranslateExtrinsic(f32m<D> &transform, const f32v<D> &translation)
{
    for (u32 i = 0; i < D; ++i)
        transform[D][i] += translation[i];
}

template <Dimension D> void ITransform<D>::ScaleIntrinsic(f32m<D> &transform, const u32 axis, const f32 scale)
{
    for (u32 i = 0; i < D; ++i)
        transform[axis][i] *= scale;
}
template <Dimension D> void ITransform<D>::ScaleIntrinsic(f32m<D> &transform, const f32v<D> &scale)
{
    for (u32 i = 0; i < D; ++i)
        for (u32 j = 0; j < D; ++j)
            transform[i][j] *= scale[i];
}

template <Dimension D> void ITransform<D>::ScaleExtrinsic(f32m<D> &transform, const u32 axis, const f32 scale)
{
    for (u32 i = 0; i < D + 1; ++i)
        transform[i][axis] *= scale;
}
template <Dimension D> void ITransform<D>::ScaleExtrinsic(f32m<D> &transform, const f32v<D> &scale)
{
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 0; j < D; ++j)
            transform[i][j] *= scale[j];
}

void Transform<D2>::RotateIntrinsic(f32m3 &transform, const f32 angle)
{
    const f32m2 rot = ComputeRotationMatrix(angle);
    const f32m2 submat = f32m2{transform} * rot;
    transform[0] = f32v3{submat[0], 0.f};
    transform[1] = f32v3{submat[1], 0.f};
}
void Transform<D2>::RotateExtrinsic(f32m3 &transform, const f32 angle)
{
    const f32m2 rot = ComputeRotationMatrix(angle);
    const f32m3x2 submat = rot * f32m3x2{transform};
    transform = f32m3{f32v3{submat[0], 0.f}, f32v3{submat[1], 0.f}, f32v3{submat[2], 1.f}};
}

void Transform<D3>::RotateXIntrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(angle);
    const f32m2x3 submat = f32m2x3{f32v3{transform[1]}, f32v3{transform[2]}} * rot;
    transform[1] = f32v4{submat[0], 0.f};
    transform[2] = f32v4{submat[1], 0.f};
}
void Transform<D3>::RotateYIntrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(-angle);
    const f32m2x3 submat = f32m2x3{f32v3{transform[0]}, f32v3{transform[2]}} * rot;
    transform[0] = f32v4{submat[0], 0.f};
    transform[2] = f32v4{submat[1], 0.f};
}
void Transform<D3>::RotateZIntrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(angle);
    const f32m2x3 submat = f32m2x3{f32v3{transform[0]}, f32v3{transform[1]}} * rot;
    transform[0] = f32v4{submat[0], 0.f};
    transform[1] = f32v4{submat[1], 0.f};
}

void Transform<D3>::RotateXExtrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(angle);
    const f32m4x2 submat =
        rot * f32m4x2{f32v2{transform[0][1], transform[0][2]}, f32v2{transform[1][1], transform[1][2]},
                      f32v2{transform[2][1], transform[2][2]}, f32v2{transform[3][1], transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        transform[i][1] = submat[i][0];
        transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateYExtrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(-angle);
    const f32m4x2 submat =
        rot * f32m4x2{f32v2{transform[0][0], transform[0][2]}, f32v2{transform[1][0], transform[1][2]},
                      f32v2{transform[2][0], transform[2][2]}, f32v2{transform[3][0], transform[3][2]}};
    for (u32 i = 0; i < 4; ++i)
    {
        transform[i][0] = submat[i][0];
        transform[i][2] = submat[i][1];
    }
}
void Transform<D3>::RotateZExtrinsic(f32m4 &transform, const f32 angle)
{
    const f32m2 rot = Transform<D2>::ComputeRotationMatrix(angle);
    const f32m4x2 submat = rot * f32m4x2{transform};
    for (u32 i = 0; i < 4; ++i)
    {
        transform[i][0] = submat[i][0];
        transform[i][1] = submat[i][1];
    }
}

void Transform<D3>::RotateIntrinsic(f32m4 &transform, const f32q &quaternion)
{
    const f32m3 rot = ComputeRotationMatrix(quaternion);
    const f32m3 submat = f32m3{transform} * rot;
    transform[0] = f32v4{submat[0], 0.f};
    transform[1] = f32v4{submat[1], 0.f};
    transform[2] = f32v4{submat[2], 0.f};
}
void Transform<D3>::RotateExtrinsic(f32m4 &transform, const f32q &quaternion)
{
    const f32m3 rot = ComputeRotationMatrix(quaternion);
    const f32m4x3 submat = rot * f32m4x3{transform};
    transform = f32m4{f32v4{submat[0], 0.f}, f32v4{submat[1], 0.f}, f32v4{submat[2], 0.f}, f32v4{submat[3], 1.f}};
}

template <Dimension D>
void ITransform<D>::Extract(const f32m<D> &transform, f32v<D> *translation, f32v<D> *scale, rot<D> *rotation)
{
    *translation = ExtractTranslation(transform);
    *scale = ExtractScale(transform);
    *rotation = ExtractRotation(transform);
}

Transform<D2> Transform<D2>::Extract(const f32m3 &ptransform)
{
    Transform<D2> transform;
    Extract(ptransform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}
Transform<D3> Transform<D3>::Extract(const f32m4 &ptransform)
{
    Transform<D3> transform;
    Extract(ptransform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

Transform<D3> Transform<D2>::Promote(const Transform &ptransform)
{
    Transform<D3> transform;
    transform.Translation = f32v3{ptransform.Translation, 0.f};
    transform.Scale = f32v3{ptransform.Scale, 1.f};
    transform.Rotation = f32q{f32v3{0.f, 0.f, ptransform.Rotation}};
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
f32m4 Transform<D2>::Promote(const f32m3 &transform)
{
    f32m4 t4 = f32m4::Identity();
    t4[0][0] = transform[0][0];
    t4[0][1] = transform[0][1];
    t4[1][0] = transform[1][0];
    t4[1][1] = transform[1][1];

    t4[3][0] = transform[2][0];
    t4[3][1] = transform[2][1];
    return t4;
}

template <Dimension D> f32v<D> ITransform<D>::ExtractTranslation(const f32m<D> &transform)
{
    return f32v<D>{transform[D]};
}
template <Dimension D> f32v<D> ITransform<D>::ExtractScale(const f32m<D> &transform)
{
    if constexpr (D == D2)
        return f32v2{Math::Norm(f32v2{transform[0]}), Math::Norm(f32v2{transform[1]})};
    else
        return f32v3{Math::Norm(f32v3{transform[0]}), Math::Norm(f32v3{transform[1]}), Math::Norm(f32v3{transform[2]})};
}
template <Dimension D> rot<D> ITransform<D>::ExtractRotation(const f32m<D> &transform)
{
    if constexpr (D == D2)
        return Math::AntiTangent(transform[0][1], transform[0][0]);
    else
    {
        f32v3 angles;
        angles[0] = Math::AntiTangent(transform[1][2], transform[2][2]);
        angles[1] = Math::AntiTangent(
            -transform[0][2], Math::SquareRoot(transform[1][2] * transform[1][2] + transform[2][2] * transform[2][2]));
        angles[2] = Math::AntiTangent(transform[0][1], transform[0][0]);
        return f32q{angles};
    }
}

template struct ITransform<D2>;
template struct ITransform<D3>;

} // namespace Onyx
