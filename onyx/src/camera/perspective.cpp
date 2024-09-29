#include "core/pch.hpp"
#include "onyx/camera/perspective.hpp"

namespace ONYX
{
Perspective3D::Perspective3D(const f32 p_Aspect, const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far)
    : Near(p_Near), Far(p_Far), FieldOfView(p_FieldOfView), m_Aspect(p_Aspect)
{
}

mat4 Perspective3D::ComputeProjectionView() const noexcept
{
    const f32 halfPov = glm::tan(0.5f * FieldOfView);

    mat4 perspective{0.f};
    perspective[0][0] = 1.f / (m_Aspect * halfPov);
    perspective[1][1] = 1.f / halfPov;
    perspective[2][2] = Far / (Far - Near);
    perspective[2][3] = 1.f;
    perspective[3][2] = Far * Near / (Near - Far);
    return perspective * Transform.ComputeViewTransform();
}

mat4 Perspective3D::ComputeInverseProjectionView() const noexcept
{
    const f32 halfPov = glm::tan(0.5f * FieldOfView);

    mat4 invPerspective{0.f};
    invPerspective[0][0] = m_Aspect * halfPov;
    invPerspective[1][1] = halfPov;
    invPerspective[3][3] = 1.f / Near;
    invPerspective[3][2] = 1.f;
    invPerspective[2][3] = (Near - Far) / (Far * Near);
    return Transform.ComputeInverseViewTransform() * invPerspective;
}

void Perspective3D::SetAspectRatio(const f32 p_Aspect) noexcept
{
    m_Aspect = p_Aspect;
}

} // namespace ONYX