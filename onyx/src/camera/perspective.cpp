#include "core/pch.hpp"
#include "onyx/camera/perspective.hpp"

namespace ONYX
{
Perspective3D::Perspective3D(const f32 p_Aspect, const f32 p_FieldOfView, const glm::mat3 &p_Rotation, const f32 p_Near,
                             const f32 p_Far)
    : Perspective3D(vec3{0.f}, p_Aspect, p_FieldOfView, p_Rotation, p_Near, p_Far)
{
}

Perspective3D::Perspective3D(const vec3 &p_Position, const f32 p_Aspect, const f32 p_FieldOfView,
                             const mat3 &p_Rotation, const f32 p_Near, const f32 p_Far)
    : Near(p_Near), Far(p_Far), FieldOfView(p_FieldOfView), m_Aspect(p_Aspect)
{
    Transform.Position = p_Position;
    Transform.Rotation = p_Rotation;
}

void Perspective3D::UpdateMatrices() noexcept
{
    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
    const float halfPov = glm::tan(0.5f * FieldOfView);

    glm::mat4 perspective = glm::mat4{0.0f};
    perspective[0][0] = 1.f / (m_Aspect * halfPov);
    perspective[1][1] = 1.f / halfPov;
    perspective[2][2] = Far / (Far - Near);
    perspective[2][3] = 1.f;
    perspective[3][2] = Far * Near / (Near - Far);

    glm::mat4 inv_perspective = glm::mat4{0.0f};
    inv_perspective[0][0] = m_Aspect * halfPov;
    inv_perspective[1][1] = halfPov;
    inv_perspective[3][3] = 1.f / Near;
    inv_perspective[3][2] = 1.f;
    inv_perspective[2][3] = (Near - Far) / (Far * Near);

    m_Projection = perspective * Transform.CameraTransform();
    m_InverseProjection = Transform.InverseCameraTransform() * inv_perspective;

    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
}

void Perspective3D::KeepAspectRatio(const f32 p_Aspect) noexcept
{
    m_Aspect = p_Aspect;
}

} // namespace ONYX