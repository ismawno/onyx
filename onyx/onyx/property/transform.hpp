#pragma once

#include "onyx/core/math.hpp"
#include "onyx/core/dimension.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"

namespace Onyx
{

enum TransformMode : u8
{
    Transform_Extrinsic,
    Transform_Intrinsic,
};

template <Dimension D> struct ITransform
{
    /**
     * @brief Compute a transformation matrix from translation, scale, and rotation.
     *
     * The order of transformations is scale -> rotate -> translate.
     */
    static f32m<D> ComputeTransform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation);

    /**
     * @brief Compute a reversed transformation matrix from translation, scale, and rotation.
     *
     * The order of transformations is translate -> rotate -> scale, hence the reverse.
     */
    static f32m<D> ComputeReversedTransform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation);

    /**
     * @brief Compute an inversed transformation matrix.
     *
     */
    static f32m<D> ComputeInverseTransform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation);

    /**
     * @brief Compute an inversed and reversed transformation matrix.
     *
     */
    static f32m<D> ComputeInverseReversedTransform(const f32v<D> &translation, const f32v<D> &scale,
                                                   const rot<D> &rotation);

    static auto ComputeRotationMatrix(const rot<D> &rotation)
    {
        if constexpr (D == D2)
        {
            const f32 c = Math::Cosine(rotation);
            const f32 s = Math::Sine(rotation);
            return f32m2{c, s, -s, c};
        }
        else
            return Math::ToMat3(rotation);
    }

    /**
     * @brief Compute an inversed rotation matrix.
     *
     */
    static auto ComputeInverseRotationMatrix(const rot<D> &rotation)
    {
        if constexpr (D == D2)
            return ComputeRotationMatrix(-rotation);
        else
            return Math::ToMat3(Math::Conjugate(rotation));
    }

    /**
     * @brief Compute the transformation matrix using the current object's translation, scale, and rotation.
     *
     * The order of transformations is scale -> rotate -> translate.
     */
    f32m<D> ComputeTransform() const;

    /**
     * @brief Compute the reversed transformation matrix using the current object's translation, scale, and rotation.
     *
     * The order of transformations is translate -> rotate -> scale, hence the reverse.
     */
    f32m<D> ComputeReversedTransform() const;

    /**
     * @brief Compute the inverse of the transformation matrix using the current object's parameters.
     *
     */
    f32m<D> ComputeInverseTransform() const;

    /**
     * @brief Compute the inverse of the axes transformation matrix using the current object's parameters.
     *
     */
    f32m<D> ComputeInverseReversedTransform() const;

    /**
     * @brief Applies an intrinsic translation to a transformation matrix along a specified axis.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     */
    static void TranslateIntrinsic(f32m<D> &transform, const u32 axis, const f32 translation)
    {
        for (u32 i = 0; i < D; ++i)
            transform[D][i] += transform[axis][i] * translation;
    }

    /**
     * @brief Applies an intrinsic translation to a transformation matrix.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     */
    static void TranslateIntrinsic(f32m<D> &transform, const f32v<D> &translation)
    {
        for (u32 i = 0; i < D; ++i)
            TranslateIntrinsic(transform, i, translation[i]);
    }

    /**
     * @brief Applies an extrinsic translation to a transformation matrix along a specified axis.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     */
    static void TranslateExtrinsic(f32m<D> &transform, const u32 axis, const f32 translation)
    {
        transform[D][axis] += translation;
    }

    /**
     * @brief Applies an extrinsic translation to a transformation matrix.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     */
    static void TranslateExtrinsic(f32m<D> &transform, const f32v<D> &translation)
    {
        for (u32 i = 0; i < D; ++i)
            transform[D][i] += translation[i];
    }

    /**
     * @brief Applies an intrinsic scaling to a transformation matrix along a specified axis.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     */
    static void ScaleIntrinsic(f32m<D> &transform, const u32 axis, const f32 scale)
    {
        for (u32 i = 0; i < D; ++i)
            transform[axis][i] *= scale;
    }

    /**
     * @brief Applies an intrinsic scaling to a transformation matrix.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     */
    static void ScaleIntrinsic(f32m<D> &transform, const f32v<D> &scale)
    {
        for (u32 i = 0; i < D; ++i)
            for (u32 j = 0; j < D; ++j)
                transform[i][j] *= scale[i];
    }

    /**
     * @brief Applies an extrinsic scaling to a transformation matrix along a specified axis.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     */
    static void ScaleExtrinsic(f32m<D> &transform, const u32 axis, const f32 scale)
    {
        for (u32 i = 0; i < D + 1; ++i)
            transform[i][axis] *= scale;
    }

    /**
     * @brief Applies an extrinsic scaling to a transformation matrix.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     */
    static void ScaleExtrinsic(f32m<D> &transform, const f32v<D> &scale)
    {
        for (u32 i = 0; i < D + 1; ++i)
            for (u32 j = 0; j < D; ++j)
                transform[i][j] *= scale[j];
    }

    /**
     * @brief Extracts translation, scale, and rotation components from a transformation matrix.
     *
     */
    static void Extract(const f32m<D> &transform, f32v<D> *translation, f32v<D> *scale, rot<D> *rotation);

    /**
     * @brief Extracts the translation component from a transformation matrix.
     *
     */
    static f32v<D> ExtractTranslation(const f32m<D> &transform);

    /**
     * @brief Extracts the scale component from a transformation matrix.
     *
     */
    static f32v<D> ExtractScale(const f32m<D> &transform);

    /**
     * @brief Extracts the rotation component from a transformation matrix.
     *
     */
    static rot<D> ExtractRotation(const f32m<D> &transform);

    f32v<D> Translation{0.f};
    f32v<D> Scale{1.f};
    rot<D> Rotation = Detail::RotType<D>::Identity;
};

template <Dimension D> struct Transform;

/**
 * @brief Specialization of Transform for 3D transformations.
 *
 * Provides additional methods specific to 3D transformations.
 */
template <> struct Transform<D3> : ITransform<D3>
{
    TKIT_REFLECT_DECLARE(Transform, ITransform<D3>)
    TKIT_YAML_SERIALIZE_DECLARE(Transform, ITransform<D3>)
    using ITransform<D3>::Extract;

    /**
     * @brief Applies an intrinsic rotation around the X-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     */
    static void RotateXIntrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an intrinsic rotation around the Y-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     */
    static void RotateYIntrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an intrinsic rotation around the Z-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     */
    static void RotateZIntrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an extrinsic rotation around the X-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     */
    static void RotateXExtrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an extrinsic rotation around the Y-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     */
    static void RotateYExtrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an extrinsic rotation around the Z-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     */
    static void RotateZExtrinsic(f32m4 &transform, f32 angle);

    /**
     * @brief Applies an intrinsic rotation using a quaternion to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param transform The transformation matrix to modify.
     * @param quaternion The quaternion representing the rotation.
     */
    static void RotateIntrinsic(f32m4 &transform, const f32q &quaternion)
    {
        const f32m3 rot = ComputeRotationMatrix(quaternion);
        const f32m3 submat = f32m3{transform} * rot;
        transform[0] = f32v4{submat[0], 0.f};
        transform[1] = f32v4{submat[1], 0.f};
        transform[2] = f32v4{submat[2], 0.f};
    }

    /**
     * @brief Applies an extrinsic rotation using a quaternion to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param transform The transformation matrix to modify.
     * @param quaternion The quaternion representing the rotation.
     */
    static void RotateExtrinsic(f32m4 &transform, const f32q &quaternion)
    {
        const f32m3 rot = ComputeRotationMatrix(quaternion);
        const f32m4x3 submat = rot * f32m4x3{transform};
        transform = f32m4{f32v4{submat[0], 0.f}, f32v4{submat[1], 0.f}, f32v4{submat[2], 0.f}, f32v4{submat[3], 1.f}};
    }

    static f32m4 ComputeLineTransform(const f32v3 &start, const f32v3 &end, f32 thickness = 1.f);

    /**
     * @brief Extracts a 3D Transform object from a transformation matrix.
     *
     * @param transform The transformation matrix.
     * @return The extracted Transform object.
     */
    static Transform Extract(const f32m4 &transform);
};

/**
 * @brief Specialization of Transform for 2D transformations.
 *
 * Provides additional methods specific to 2D transformations.
 */
template <> struct Transform<D2> : ITransform<D2>
{
    TKIT_REFLECT_DECLARE(Transform, ITransform<D2>)
    TKIT_YAML_SERIALIZE_DECLARE(Transform, ITransform<D2>)
    using ITransform<D2>::Extract;

    /**
     * @brief Applies an intrinsic rotation to a 2D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     */
    static void RotateIntrinsic(f32m3 &transform, f32 angle);

    /**
     * @brief Applies an extrinsic rotation to a 2D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     */
    static void RotateExtrinsic(f32m3 &transform, f32 angle);

    /**
     * @brief Extracts a 2D Transform object from a transformation matrix.
     *
     * @param transform The transformation matrix.
     * @return The extracted Transform object.
     */
    static Transform Extract(const f32m3 &transform);

    /**
     * @brief Promote a 2D transform to an equivalent 3D transform.
     *
     * This function takes a 2D transform and promotes it to a 3D transform by adding a Z-axis component set to the
     * identity.
     *
     * @param transform The 2D transform to promote.
     * @return The promoted 3D transform.
     */
    static Transform<D3> Promote(const Transform &transform);

    /**
     * @brief Promote a 2D transform to an equivalent 3D transform.
     *
     * This function takes a 2D transform and promotes it to a 3D transform by adding a Z-axis component set to the
     * identity.
     *
     * @param transform The 2D transform to promote.
     * @return The promoted 3D transform.
     */
    static f32m4 Promote(const f32m3 &transform);

    /**
     * @brief Promote a 2D transform to an equivalent 3D transform.
     *
     * This function promotes a 2D transform into a 3D transform by adding a Z-axis component set to the
     * identity.
     *
     * @return The promoted 3D transform.
     */
    Transform<D3> Promote();
};

} // namespace Onyx
