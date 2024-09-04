#include "core/pch.hpp"
#include "onyx/camera/camera.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE void Camera<N>::KeepAspectRatio(const float p_Aspect) noexcept
{
    Transform.Scale.x = p_Aspect * Transform.Scale.y;
}

ONYX_DIMENSION_TEMPLATE vec<N> Camera<N>::ScreenToWorld(const vec2 &p_Screen) const noexcept
{
    // The z component is set to 0.5f as sort of a midpoint. This works well with 2D. Not so sure about 3D
    // TODO: Check if this works for 3D
    const vec4 world = m_InverseProjection * vec4(p_Screen, 0.5f, 1.f);
    return vec<N>(world) * world.w;
}

ONYX_DIMENSION_TEMPLATE vec2 Camera<N>::WorldToScreen(const vec<N> &p_World) const noexcept
{
    if constexpr (N == 2)
    {
        const vec4 pos = m_Projection * vec4(p_World, 0.5f, 1.f);
        return vec2(pos) / pos.w;
    }
    else
    {
        const vec4 pos = m_Projection * vec4(p_World, 1.f);
        return vec2(pos) / pos.w;
    }
}

ONYX_DIMENSION_TEMPLATE void Camera<N>::FlipY() noexcept
{
    m_YFlipped = !m_YFlipped;
}

ONYX_DIMENSION_TEMPLATE const mat4 &Camera<N>::Projection() const noexcept
{
    return m_Projection;
}
ONYX_DIMENSION_TEMPLATE const mat4 &Camera<N>::InverseProjection() const noexcept
{
    return m_InverseProjection;
}

void Camera3D::PointTowards(const vec3 &p_Direction) noexcept
{
    const float roty = glm::atan(p_Direction.x, p_Direction.z);
    const float rotx = -glm::atan(p_Direction.y, p_Direction.z * cosf(roty) + p_Direction.x * sinf(roty));
    Transform.Rotation = ONYX::Transform3D::RotYX(roty, rotx);
}
void Camera3D::PointTo(const vec3 &p_Location) noexcept
{
    PointTowards(p_Location - Transform.Position);
}

template class Camera<2>;

} // namespace ONYX