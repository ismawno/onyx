#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/alias.hpp"

namespace Onyx
{
/**
 * @brief Interface for transformation operations in D-dimensional space.
 *
 * Provides methods for computing transformations, inverse transformations,
 * and extracting translation, scale, and rotation components from matrices.
 */
template <Dimension D> struct ONYX_API ITransform
{
    /**
     * @brief Compute a transformation matrix from translation, scale, and rotation.
     *
     * The order of transformations is scale -> rotate -> translate.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     * @return The resulting transformation matrix.
     */
    static mat<D> ComputeTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                   const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Compute a reversed transformation matrix from translation, scale, and rotation.
     *
     * The order of transformations is translate -> rotate -> scale, hence the reverse.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     * @return The resulting transformation matrix.
     */
    static mat<D> ComputeReversedTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                           const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Compute an inversed transformation matrix.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     * @return The inverse transformation matrix.
     */
    static mat<D> ComputeInverseTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                          const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Compute an inversed and reversed transformation matrix.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     * @return The inverse transformation matrix.
     */
    static mat<D> ComputeInverseReversedTransform(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                                  const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Compute a rotation matrix.
     *
     */
    static auto ComputeRotationMatrix(const rot<D> &p_Rotation) noexcept
    {
        if constexpr (D == D2)
        {
            const f32 c = glm::cos(p_Rotation);
            const f32 s = glm::sin(p_Rotation);
            return fmat2{c, s, -s, c};
        }
        else
            return glm::toMat3(p_Rotation);
    }

    /**
     * @brief Compute an inversed rotation matrix.
     *
     */
    static auto ComputeInverseRotationMatrix(const rot<D> &p_Rotation) noexcept
    {
        if constexpr (D == D2)
            return ComputeRotationMatrix(-p_Rotation);
        else
            return glm::toMat3(glm::conjugate(p_Rotation));
    }

    /**
     * @brief Compute the transformation matrix using the current object's translation, scale, and rotation.
     *
     * The order of transformations is scale -> rotate -> translate.
     *
     * @return The transformation matrix.
     */
    mat<D> ComputeTransform() const noexcept;

    /**
     * @brief Compute the reversed transformation matrix using the current object's translation, scale, and rotation.
     *
     * The order of transformations is translate -> rotate -> scale, hence the reverse.
     *
     * @return The transformation matrix.
     */
    mat<D> ComputeReversedTransform() const noexcept;

    /**
     * @brief Compute the inverse of the transformation matrix using the current object's parameters.
     *
     * @return The inverse transformation matrix.
     */
    mat<D> ComputeInverseTransform() const noexcept;

    /**
     * @brief Compute the inverse of the axes transformation matrix using the current object's parameters.
     *
     * @return The inverse transformation matrix.
     */
    mat<D> ComputeInverseReversedTransform() const noexcept;

    /**
     * @brief Applies an intrinsic translation to a transformation matrix along a specified axis.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Axis The axis index along which to translate.
     * @param p_Translation The translation amount.
     */
    static void TranslateIntrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;

    /**
     * @brief Applies an intrinsic translation to a transformation matrix.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Translation The translation vector.
     */
    static void TranslateIntrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept;

    /**
     * @brief Applies an extrinsic translation to a transformation matrix along a specified axis.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Axis The axis index along which to translate.
     * @param p_Translation The translation amount.
     */
    static void TranslateExtrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Translation) noexcept;

    /**
     * @brief Applies an extrinsic translation to a transformation matrix.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Translation The translation vector.
     */
    static void TranslateExtrinsic(mat<D> &p_Transform, const vec<D> &p_Translation) noexcept;

    /**
     * @brief Applies an intrinsic scaling to a transformation matrix along a specified axis.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Axis The axis index along which to scale.
     * @param p_Scale The scaling factor.
     */
    static void ScaleIntrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;

    /**
     * @brief Applies an intrinsic scaling to a transformation matrix.
     *
     * Intrinsic transformations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Scale The scaling vector.
     */
    static void ScaleIntrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept;

    /**
     * @brief Applies an extrinsic scaling to a transformation matrix along a specified axis.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Axis The axis index along which to scale.
     * @param p_Scale The scaling factor.
     */
    static void ScaleExtrinsic(mat<D> &p_Transform, u32 p_Axis, f32 p_Scale) noexcept;

    /**
     * @brief Applies an extrinsic scaling to a transformation matrix.
     *
     * Extrinsic transformations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Scale The scaling vector.
     */
    static void ScaleExtrinsic(mat<D> &p_Transform, const vec<D> &p_Scale) noexcept;

    /**
     * @brief Extracts translation, scale, and rotation components from a transformation matrix.
     *
     * @param p_Transform The transformation matrix to extract from.
     * @param p_Translation Output pointer for the translation vector.
     * @param p_Scale Output pointer for the scale vector.
     * @param p_Rotation Output pointer for the rotation object.
     */
    static void Extract(const mat<D> &p_Transform, vec<D> *p_Translation, vec<D> *p_Scale, rot<D> *p_Rotation) noexcept;

    /**
     * @brief Extracts the translation component from a transformation matrix.
     *
     * @param p_Transform The transformation matrix.
     * @return The extracted translation vector.
     */
    static vec<D> ExtractTranslation(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Extracts the scale component from a transformation matrix.
     *
     * @param p_Transform The transformation matrix.
     * @return The extracted scale vector.
     */
    static vec<D> ExtractScale(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Extracts the rotation component from a transformation matrix.
     *
     * @param p_Transform The transformation matrix.
     * @return The extracted rotation object.
     */
    static rot<D> ExtractRotation(const mat<D> &p_Transform) noexcept;

    /// Translation vector component of the transformation.
    vec<D> Translation{0.f};

    /// Scale vector component of the transformation.
    vec<D> Scale{1.f};

    /// Rotation component of the transformation.
    rot<D> Rotation = RotType<D>::Identity;
};

template <Dimension D> struct Transform;

/**
 * @brief Specialization of Transform for 2D transformations.
 *
 * Provides additional methods specific to 2D transformations.
 */
template <> struct ONYX_API Transform<D2> : ITransform<D2>
{
    using ITransform<D2>::Extract;

    /**
     * @brief Applies an intrinsic rotation to a 2D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateIntrinsic(fmat3 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an extrinsic rotation to a 2D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateExtrinsic(fmat3 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Extracts a 2D Transform object from a transformation matrix.
     *
     * @param p_Transform The transformation matrix.
     * @return The extracted Transform object.
     */
    static Transform Extract(const fmat3 &p_Transform) noexcept;
};

/**
 * @brief Specialization of Transform for 3D transformations.
 *
 * Provides additional methods specific to 3D transformations.
 */
template <> struct ONYX_API Transform<D3> : ITransform<D3>
{
    using ITransform<D3>::Extract;

    /**
     * @brief Applies an intrinsic rotation around the X-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateXIntrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an intrinsic rotation around the Y-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateYIntrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an intrinsic rotation around the Z-axis to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateZIntrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an extrinsic rotation around the X-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateXExtrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an extrinsic rotation around the Y-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateYExtrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an extrinsic rotation around the Z-axis to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Angle The rotation angle in radians.
     */
    static void RotateZExtrinsic(fmat4 &p_Transform, f32 p_Angle) noexcept;

    /**
     * @brief Applies an intrinsic rotation using a quaternion to a 3D transformation matrix.
     *
     * Intrinsic rotations are applied relative to the object's local coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Quaternion The quaternion representing the rotation.
     */
    static void RotateIntrinsic(fmat4 &p_Transform, const quat &p_Quaternion) noexcept;

    /**
     * @brief Applies an extrinsic rotation using a quaternion to a 3D transformation matrix.
     *
     * Extrinsic rotations are applied relative to the global coordinate system.
     *
     * @param p_Transform The transformation matrix to modify.
     * @param p_Quaternion The quaternion representing the rotation.
     */
    static void RotateExtrinsic(fmat4 &p_Transform, const quat &p_Quaternion) noexcept;

    /**
     * @brief Extracts a 3D Transform object from a transformation matrix.
     *
     * @param p_Transform The transformation matrix.
     * @return The extracted Transform object.
     */
    static Transform Extract(const fmat4 &p_Transform) noexcept;
};

} // namespace Onyx
