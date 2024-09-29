#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Transform;
ONYX_DIMENSION_TEMPLATE class ITransform
{
  public:
    const vec<N> &GetPosition() const noexcept;
    const vec<N> &GetScale() const noexcept;
    const vec<N> &GetOrigin() const noexcept;

    f32 *GetPositionPtr() noexcept;
    f32 *GetScalePtr() noexcept;
    f32 *GetOriginPtr() noexcept;

    const mat4 &GetLocalTransform() const noexcept;
    const mat4 &GetGlobalTransform() const noexcept;

    mat4 ComputeInverseLocalModelTransform() const noexcept;  // always up to date
    mat4 ComputeInverseLocalCameraTransform() const noexcept; // always up to date

    mat4 ComputeInverseGlobalModelTransform() const noexcept;  // always up to date
    mat4 ComputeInverseGlobalCameraTransform() const noexcept; // always up to date

    const Transform<N> *GetParent() const noexcept;
    Transform<N> *GetParent() noexcept;

    void SetParent(Transform<N> *p_Parent) noexcept;

    void SetPosition(const vec<N> &p_Position) noexcept;
    void SetPositionX(f32 p_Position) noexcept;
    void SetPositionY(f32 p_Position) noexcept;

    void SetScale(const vec<N> &p_Scale) noexcept;
    void SetScaleX(f32 p_Scale) noexcept;
    void SetScaleY(f32 p_Scale) noexcept;

    void SetOrigin(const vec<N> &p_Origin) noexcept;
    void SetOriginX(f32 p_Origin) noexcept;
    void SetOriginY(f32 p_Origin) noexcept;

    void SetLocalTransform(const mat4 &p_LocalTransform) noexcept;
    void SetParentTransform(const mat4 &p_ParentTransform) noexcept;

    void UpdateMatricesAsModel() noexcept;
    void UpdateMatricesAsCamera() noexcept;

    void UpdateMatricesAsModelForced() noexcept;
    void UpdateMatricesAsCameraForced() noexcept;

    // The origins do have to match for consistency
    void UpdateComponents() noexcept;

    vec<N> LocalOffset(const vec<N> &p_Offset) const noexcept;
    vec<N> LocalOffsetX(f32 p_Offset) const noexcept;
    vec<N> LocalOffsetY(f32 p_Offset) const noexcept;

    bool NeedsMatrixUpdate() const noexcept;
    bool NeedsComponentsUpdate() const noexcept;

  protected:
    vec<N> m_Position{0.f};
    vec<N> m_Scale{1.f};
    vec<N> m_Origin{0.f};
    rot<N> m_Rotation = RotType<N>::Identity;
    Transform<N> *m_Parent = nullptr;

    mat4 m_LocalTransform{1.f};
    mat4 m_GlobalTransform{1.f};

    bool m_NeedsMatrixUpdate = false;
    bool m_NeedsComponentsUpdate = false;
};

// Should add a GetGlobalPosition()?
// Only caches one type: either model or camera

template <> class ONYX_API Transform<2> final : public ITransform<2>
{
  public:
    f32 GetRotation() const noexcept;
    f32 *GetRotationPtr() noexcept;

    void SetRotation(f32 p_Rotation) noexcept;
};

template <> class ONYX_API Transform<3> : public ITransform<3>
{
  public:
    const quat &GetQuaternion() const noexcept;
    vec3 GetEulerAngles() const noexcept;

    void SetPositionZ(f32 p_Position) noexcept;
    void SetScaleZ(f32 p_Scale) noexcept;
    void SetOriginZ(f32 p_Origin) noexcept;

    void SetRotation(const quat &p_Rotation) noexcept;
    void SetRotation(const vec3 &p_Angles) noexcept;

    vec3 LocalOffsetZ(f32 p_Offset) const noexcept;

    void RotateLocalAxis(const quat &p_Rotation) noexcept;
    void RotateLocalAxis(const vec3 &p_Angles) noexcept;
    void RotateLocalAxisX(f32 p_Angle) noexcept;
    void RotateLocalAxisY(f32 p_Angle) noexcept;
    void RotateLocalAxisZ(f32 p_Angle) noexcept;

    void RotateGlobalAxis(const quat &p_Rotation) noexcept;
    void RotateGlobalAxis(const vec3 &p_Angles) noexcept;
    void RotateGlobalAxisX(f32 p_Angle) noexcept;
    void RotateGlobalAxisY(f32 p_Angle) noexcept;
    void RotateGlobalAxisZ(f32 p_Angle) noexcept;
};

using Transform2D = Transform<2>;
using Transform3D = Transform<3>;

} // namespace ONYX