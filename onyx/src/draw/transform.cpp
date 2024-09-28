#include "core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/core/logging.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Trigonometry;

template <> struct Trigonometry<2>
{
    Trigonometry(f32 p_Rotation) noexcept : c(glm::cos(p_Rotation)), s(glm::sin(p_Rotation))
    {
    }

    mat2 RotationMatrix() const noexcept
    {
        return mat2(c, -s, s, c);
    }

    f32 c;
    f32 s;
};

// Potentially unused
template <> struct Trigonometry<3>
{
    Trigonometry(const vec3 &p_Rotation) noexcept
        : c1(glm::cos(p_Rotation.x)), s1(glm::sin(p_Rotation.x)), c2(glm::cos(p_Rotation.y)),
          s2(glm::sin(p_Rotation.y)), c3(glm::cos(p_Rotation.z)), s3(glm::sin(p_Rotation.z))
    {
    }

    f32 c1;
    f32 s1;
    f32 c2;
    f32 s2;
    f32 c3;
    f32 s3;
};

static mat4 computeModelTransform(const vec2 &p_Position, const vec2 &p_Scale, const vec2 &p_Origin,
                                  const f32 p_Rotation) noexcept
{
    const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
    const mat2 rotScale = Trigonometry<2>(p_Rotation).RotationMatrix() * scale;
    const vec2 translation = p_Position - rotScale * p_Origin;

    return mat4{vec4(rotScale[0], 0.f, 0.f), vec4(rotScale[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                vec4(translation, 0.f, 1.f)};
}

static mat4 computeInverseModelTransform(const vec2 &p_Position, const vec2 &p_Scale, const vec2 &p_Origin,
                                         const f32 p_Rotation) noexcept
{
    const mat2 scale = mat2({1.f / p_Scale.x, 0.f}, {0.f, 1.f / p_Scale.y});
    const mat2 rotScale = scale * Trigonometry<2>(-p_Rotation).RotationMatrix();
    const vec2 translation = p_Origin - rotScale * p_Position;

    return mat4{vec4(rotScale[0], 0.f, 0.f), vec4(rotScale[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                vec4(translation, 0.f, 1.f)};
}

static mat4 computeCameraTransform(const vec2 &p_Position, vec2 p_Scale, const vec2 &p_Origin,
                                   const f32 p_Rotation) noexcept
{
    p_Scale.y = -p_Scale.y; // Flip the y-axis. I want a right-handed coordinate system
    const mat2 rotation = Trigonometry<2>(-p_Rotation).RotationMatrix();
    const vec2 translation = (p_Origin - rotation * p_Position) / p_Scale;

    return mat4{vec4(rotation[0] / p_Scale, 0.f, 0.f), vec4(rotation[1] / p_Scale, 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                vec4(translation, 0.f, 1.f)};
}

static mat4 computeInverseCameraTransform(const vec2 &p_Position, vec2 p_Scale, const vec2 &p_Origin,
                                          const f32 p_Rotation) noexcept
{
    p_Scale.y = -p_Scale.y; // Flip the y-axis. I want a right-handed coordinate system
    const mat2 rotation = Trigonometry<2>(-p_Rotation).RotationMatrix();
    const vec2 translation = p_Position - rotation * p_Origin;

    return mat4{vec4(rotation[0] * p_Scale.x, 0.f, 0.f), vec4(rotation[1] * p_Scale.y, 0.f, 0.f),
                vec4(0.f, 0.f, 1.f, 0.f), vec4(translation, 0.f, 1.f)};
}

static mat4 computeModelTransform(const vec3 &p_Position, const vec3 &p_Scale, const vec3 &p_Origin,
                                  const quat &p_Rotation) noexcept
{
    const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
    const mat3 rotScale = glm::toMat3(p_Rotation) * scale;
    const vec3 translation = p_Position - rotScale * p_Origin;

    return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4(translation, 1.f)};
}

static mat4 computeInverseModelTransform(const vec3 &p_Position, const vec3 &p_Scale, const vec3 &p_Origin,
                                         const quat &p_Rotation) noexcept
{
    const mat3 scale = mat3({1.f / p_Scale.x, 0.f, 0.f}, {0.f, 1.f / p_Scale.y, 0.f}, {0.f, 0.f, 1.f / p_Scale.z});
    const mat3 rotScale = scale * glm::toMat3(glm::conjugate(p_Rotation));
    const vec3 translation = p_Origin - rotScale * p_Position;

    return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4(translation, 1.f)};
}

static mat4 computeCameraTransform(const vec3 &p_Position, const vec3 &p_Scale, const vec3 &p_Origin,
                                   const quat &p_Rotation) noexcept
{
    const mat3 rotation = glm::toMat3(glm::conjugate(p_Rotation));
    const vec3 translation = (p_Origin - rotation * p_Position) / p_Scale;
    return mat4{vec4(rotation[0] / p_Scale, 0.f), vec4(rotation[1] / p_Scale, 0.f), vec4(rotation[2] / p_Scale, 0.f),
                vec4(translation, 1.f)};
}

static mat4 computeInverseCameraTransform(const vec3 &p_Position, const vec3 &p_Scale, const vec3 &p_Origin,
                                          const quat &p_Rotation) noexcept
{
    const mat3 rotation = glm::toMat3(p_Rotation);
    const vec3 translation = p_Position - rotation * p_Origin;
    return mat4{vec4(rotation[0] * p_Scale.x, 0.f), vec4(rotation[1] * p_Scale.y, 0.f),
                vec4(rotation[2] * p_Scale.z, 0.f), vec4(translation, 1.f)};
}

ONYX_DIMENSION_TEMPLATE const vec<N> &ITransform<N>::GetPosition() const noexcept
{
    return m_Position;
}
ONYX_DIMENSION_TEMPLATE const vec<N> &ITransform<N>::GetScale() const noexcept
{
    return m_Scale;
}
ONYX_DIMENSION_TEMPLATE const vec<N> &ITransform<N>::GetOrigin() const noexcept
{
    return m_Origin;
}

ONYX_DIMENSION_TEMPLATE const mat4 &ITransform<N>::GetLocalTransform() const noexcept
{
    return m_LocalTransform;
}
ONYX_DIMENSION_TEMPLATE const mat4 &ITransform<N>::GetGlobalTransform() const noexcept
{
    return m_Parent ? m_GlobalTransform : m_LocalTransform;
}

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseLocalModelTransform() const noexcept
{
    return computeInverseModelTransform(m_Position, m_Scale, m_Origin, m_Rotation);
}
ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseLocalCameraTransform() const noexcept
{
    return computeInverseCameraTransform(m_Position, m_Scale, m_Origin, m_Rotation);
}

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseGlobalModelTransform() const noexcept
{
    return m_Parent ? ComputeInverseGlobalModelTransform() * m_Parent->ComputeInverseLocalModelTransform()
                    : ComputeInverseLocalModelTransform();
}
ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseGlobalCameraTransform() const noexcept
{
    return m_Parent ? ComputeInverseGlobalCameraTransform() * m_Parent->ComputeInverseLocalCameraTransform()
                    : ComputeInverseLocalCameraTransform();
}

ONYX_DIMENSION_TEMPLATE const Transform<N> *ITransform<N>::GetParent() const noexcept
{
    return m_Parent;
}
ONYX_DIMENSION_TEMPLATE Transform<N> *ITransform<N>::GetParent() noexcept
{
    return m_Parent;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetParent(Transform<N> *p_Parent) noexcept
{
    m_NeedsComponentsUpdate |= m_Parent != p_Parent;
    m_NeedsMatrixUpdate |= m_NeedsComponentsUpdate;
    m_Parent = p_Parent;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetPosition(const vec<N> &p_Position) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Position = p_Position;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetPositionX(const f32 p_Position) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Position.x = p_Position;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetPositionY(const f32 p_Position) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Position.y = p_Position;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetScale(const vec<N> &p_Scale) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Scale = p_Scale;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetScaleX(const f32 p_Scale) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Scale.x = p_Scale;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetScaleY(const f32 p_Scale) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Scale.y = p_Scale;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetOrigin(const vec<N> &p_Origin) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Origin = p_Origin;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetOriginX(const f32 p_Origin) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Origin.x = p_Origin;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetOriginY(const f32 p_Origin) noexcept
{
    m_NeedsMatrixUpdate = true;
    m_Origin.y = p_Origin;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetLocalTransform(const mat4 &p_LocalTransform) noexcept
{
    m_NeedsComponentsUpdate = true;
    m_LocalTransform = p_LocalTransform;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::SetParentTransform(const mat4 &p_ParentTransform) noexcept
{
    m_GlobalTransform = p_ParentTransform * m_LocalTransform;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::UpdateMatricesAsModel() noexcept
{
    if (m_NeedsMatrixUpdate)
        m_LocalTransform = computeModelTransform(m_Position, m_Scale, m_Origin, m_Rotation);

    m_NeedsMatrixUpdate = false;
    if (!m_Parent)
        return;

    if (m_Parent->NeedsMatrixUpdate())
        m_Parent->UpdateMatricesAsModel();
    m_GlobalTransform = m_Parent->GetGlobalTransform() * m_LocalTransform;
}
ONYX_DIMENSION_TEMPLATE void ITransform<N>::UpdateMatricesAsCamera() noexcept
{
    if (m_NeedsMatrixUpdate)
        m_LocalTransform = computeCameraTransform(m_Position, m_Scale, m_Origin, m_Rotation);

    m_NeedsMatrixUpdate = false;
    if (!m_Parent)
        return;

    // TODO: Handle camera parent here: how should this propagate?
    if (m_Parent->NeedsMatrixUpdate())
        m_Parent->UpdateMatricesAsCamera();
    m_GlobalTransform = m_LocalTransform * m_Parent->GetGlobalTransform();
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::UpdateComponents() noexcept
{
    KIT_LOG_WARNING_IF(!m_NeedsComponentsUpdate, "Transform components are already up to date");
    m_NeedsComponentsUpdate = false;
    if constexpr (N == 2)
    {
        const mat2 rotScale = mat2{m_LocalTransform[0], m_LocalTransform[1]};
        m_Scale = vec2{glm::length(rotScale[0]), glm::length(rotScale[1])};

        // No need to normalize: scale cancels out
        m_Rotation = glm::atan(m_LocalTransform[0].y, m_LocalTransform[0].x);
        m_Position = vec2{m_LocalTransform[3]} + rotScale * m_Origin;
    }
    else
    {
        const mat3 rotScale = mat3{m_LocalTransform[0], m_LocalTransform[1], m_LocalTransform[2]};
        m_Scale = vec3{glm::length(rotScale[0]), glm::length(rotScale[1]), glm::length(rotScale[2])};

        vec3 angles;
        angles.x = glm::atan(rotScale[1][2], rotScale[2][2]);
        angles.y =
            glm::atan(-rotScale[0][2], glm::sqrt(rotScale[1][2] * rotScale[1][2] + rotScale[2][2] * rotScale[2][2]));
        angles.z = glm::atan(rotScale[0][1], rotScale[0][0]);

        m_Rotation = quat{angles};
        m_Position = vec3{m_LocalTransform[3]} + rotScale * m_Origin;
    }

    ONYX_DIMENSION_TEMPLATE bool ITransform<N>::NeedsMatrixUpdate() const noexcept
    {
        return m_NeedsMatrixUpdate || (m_Parent && m_Parent->NeedsMatrixUpdate());
    }
    ONYX_DIMENSION_TEMPLATE bool ITransform<N>::NeedsComponentsUpdate() const noexcept
    {
        return m_NeedsComponentsUpdate;
    }

    ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffset(const vec<N> &p_Offset) const noexcept
    {
        if constexpr (N == 2)
            return Trigonometry<2>(m_Rotation).RotationMatrix() * p_Offset;
        else
            return m_Rotation * p_Offset;
    }

    ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffsetX(const f32 p_Offset) const noexcept
    {
        if constexpr (N == 2)
        {
            const auto [c, s] = Trigonometry<2>(m_Rotation);
            return vec<N>(c * p_Offset, s * p_Offset);
        }
        else
            return LocalOffset(vec3{p_Offset, 0.f, 0.f});
    }

    ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffsetY(const f32 p_Offset) const noexcept
    {
        if constexpr (N == 2)
        {
            const auto [c, s] = Trigonometry<2>(m_Rotation);
            return vec<N>(-s * p_Offset, c * p_Offset);
        }
        else
            return LocalOffset(vec3{0.f, p_Offset, 0.f});
    }

    f32 Transform<2>::GetRotation() const noexcept
    {
        return m_Rotation;
    }
    void Transform<2>::SetRotation(const f32 p_Rotation) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Rotation = p_Rotation;
    }

    const quat &Transform<3>::GetQuaternion() const noexcept
    {
        return m_Rotation;
    }
    vec3 Transform<3>::GetEulerAngles() const noexcept
    {
        return glm::eulerAngles(m_Rotation);
    }

    void Transform<3>::SetPositionZ(const f32 p_Position) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Position.z = p_Position;
    }
    void Transform<3>::SetScaleZ(const f32 p_Scale) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Scale.z = p_Scale;
    }
    void Transform<3>::SetOriginZ(const f32 p_Origin) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Origin.z = p_Origin;
    }

    void Transform<3>::SetRotation(const quat &p_Rotation) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Rotation = p_Rotation;
    }
    void Transform<3>::SetRotation(const vec3 &p_Angles) noexcept
    {
        m_NeedsMatrixUpdate = true;
        m_Rotation = glm::quat(p_Angles);
    }

    vec3 Transform<3>::LocalOffsetZ(const f32 p_Offset) const noexcept
    {
        return LocalOffset(vec3{0.f, 0.f, p_Offset});
    }

    void Transform<3>::RotateLocalAxis(const quat &p_Rotation) noexcept
    {
        m_Rotation = glm::normalize(m_Rotation * p_Rotation);
    }
    void Transform<3>::RotateLocalAxis(const vec3 &p_Angles) noexcept
    {
        RotateLocalAxis(glm::quat(p_Angles));
    }
    void Transform<3>::RotateLocalAxisX(const f32 p_Angle) noexcept
    {
        RotateLocalAxis(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
    }
    void Transform<3>::RotateLocalAxisY(const f32 p_Angle) noexcept
    {
        RotateLocalAxis(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
    }
    void Transform<3>::RotateLocalAxisZ(const f32 p_Angle) noexcept
    {
        RotateLocalAxis(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
    }

    void Transform<3>::RotateGlobalAxis(const quat &p_Rotation) noexcept
    {
        m_Rotation = glm::normalize(p_Rotation * m_Rotation);
    }
    void Transform<3>::RotateGlobalAxis(const vec3 &p_Angles) noexcept
    {
        RotateGlobalAxis(glm::quat(p_Angles));
    }
    void Transform<3>::RotateGlobalAxisX(const f32 p_Angle) noexcept
    {
        RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
    }
    void Transform<3>::RotateGlobalAxisY(const f32 p_Angle) noexcept
    {
        RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
    }
    void Transform<3>::RotateGlobalAxisZ(const f32 p_Angle) noexcept
    {
        RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
    }

    template class ITransform<2>;
    template class ITransform<3>;

} // namespace ONYX