#include "core/pch.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{
Orthographic2D::Orthographic2D(const f32 p_Aspect, const f32 p_Size, const f32 p_Rotation)
    : Orthographic2D(vec2{0.f}, p_Aspect, p_Size, p_Rotation)
{
}

Orthographic2D::Orthographic2D(const vec2 &p_Position, const f32 p_Aspect, const f32 p_Size, const f32 p_Rotation)
    : Orthographic2D(p_Position, {p_Aspect * p_Size, p_Size}, p_Rotation)
{
}

Orthographic2D::Orthographic2D(const vec2 &p_Size, const f32 p_Rotation) : Orthographic2D(vec2{0.f}, p_Size, p_Rotation)
{
}

Orthographic2D::Orthographic2D(const vec2 &p_Position, const vec2 &p_Size, const f32 p_Rotation)
{
    Transform.Position = p_Position;
    Transform.Scale = p_Size;
    Transform.Rotation = p_Rotation;
}

void Orthographic2D::UpdateMatrices() noexcept
{
    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;

    m_Projection = Transform.CameraTransform();
    m_InverseProjection = Transform.InverseCameraTransform();

    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
}

f32 Orthographic2D::Size() const noexcept
{
    return Transform.Scale.y;
}

void Orthographic2D::Size(const f32 p_Size) noexcept
{
    const f32 aspect = Transform.Scale.x / Transform.Scale.y;
    Transform.Scale = {aspect * p_Size, p_Size};
}

Orthographic3D::Orthographic3D(const f32 p_Aspect, const f32 p_XYSize, const f32 p_Depth,
                               const mat3 &p_Rotation) noexcept
    : Orthographic3D(vec3{0.f}, p_Aspect, p_XYSize, p_Depth, p_Rotation)
{
}

Orthographic3D::Orthographic3D(const vec3 &p_Position, const f32 p_Aspect, const f32 p_XYSize, const f32 p_Depth,
                               const mat3 &p_Rotation) noexcept
    : Orthographic3D(p_Position, {p_Aspect * p_XYSize, p_XYSize, p_Depth}, p_Rotation)
{
}

Orthographic3D::Orthographic3D(const vec3 &p_Size, const mat3 &p_Rotation) noexcept
    : Orthographic3D(vec3{0.f}, p_Size, p_Rotation)
{
}

Orthographic3D::Orthographic3D(const vec3 &p_Position, const vec3 &p_Size, const mat3 &p_Rotation) noexcept
{
    Transform.Position = p_Position;
    Transform.Scale = p_Size;
    Transform.Rotation = p_Rotation;
}

void Orthographic3D::UpdateMatrices() noexcept
{
    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;

    m_Projection = Transform.CameraTransform();
    m_InverseProjection = Transform.InverseCameraTransform();

    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
}

f32 Orthographic3D::Size() const
{
    return Transform.Scale.y;
}

void Orthographic3D::Size(const f32 p_Size)
{
    const f32 aspect = Transform.Scale.x / Transform.Scale.y;
    Transform.Scale.x = aspect * p_Size;
    Transform.Scale.y = p_Size;
}

} // namespace ONYX