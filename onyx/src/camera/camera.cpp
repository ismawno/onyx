#include "core/pch.hpp"
#include "onyx/camera/camera.hpp"

namespace ONYX
{
void ICamera::FlipY() noexcept
{
    m_YFlipped = !m_YFlipped;
}

const mat4 &ICamera::GetProjection() const noexcept
{
    return m_Projection;
}
const mat4 &ICamera::GetInverseProjection() const noexcept
{
    return m_InverseProjection;
}

ONYX_DIMENSION_TEMPLATE Camera<N>::Camera(const ONYX::Transform<N> &p_Transform) noexcept : Transform(p_Transform)
{
}

ONYX_DIMENSION_TEMPLATE void Camera<N>::SetAspectRatio(const f32 p_Aspect) noexcept
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

template class Camera<2>;
template class Camera<3>;

} // namespace ONYX