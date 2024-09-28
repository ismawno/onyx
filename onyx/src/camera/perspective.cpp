#include "core/pch.hpp"
#include "onyx/camera/perspective.hpp"

namespace ONYX
{
Perspective3D::Perspective3D(const f32 p_Aspect, const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far)
    : Near(p_Near), Far(p_Far), FieldOfView(p_FieldOfView), m_Aspect(p_Aspect)
{
}

void Perspective3D::UpdateMatrices() noexcept
{
    const f32 halfPov = glm::tan(0.5f * FieldOfView);

    glm::mat4 perspective = glm::mat4{0.0f};
    perspective[0][0] = 1.f / (m_Aspect * halfPov);
    perspective[1][1] = 1.f / halfPov;
    perspective[2][2] = Far / (Far - Near);
    perspective[2][3] = 1.f;
    perspective[3][2] = Far * Near / (Near - Far);

    glm::mat4 invPerspective = glm::mat4{0.0f};
    invPerspective[0][0] = m_Aspect * halfPov;
    invPerspective[1][1] = halfPov;
    invPerspective[3][3] = 1.f / Near;
    invPerspective[3][2] = 1.f;
    invPerspective[2][3] = (Near - Far) / (Far * Near);

    if (Transform.NeedsMatrixUpdate())
        Transform.UpdateMatricesAsCamera();
    m_Projection = perspective * Transform.GetGlobalTransform();
    m_InverseProjection = Transform.ComputeInverseGlobalCameraTransform() * invPerspective;
}

void Perspective3D::SetAspectRatio(const f32 p_Aspect) noexcept
{
    m_Aspect = p_Aspect;
}

} // namespace ONYX