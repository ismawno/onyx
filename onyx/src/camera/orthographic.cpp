#include "core/pch.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{

Orthographic<2>::Orthographic(const f32 p_Aspect, const f32 p_Size, const f32 p_Rotation)
    : Orthographic(vec2{0.f}, p_Aspect, p_Size, p_Rotation)
{
}

Orthographic<2>::Orthographic(const vec2 &p_Position, const f32 p_Aspect, const f32 p_Size, const f32 p_Rotation)
    : Orthographic(p_Position, {p_Aspect * p_Size, p_Size}, p_Rotation)
{
}

Orthographic<2>::Orthographic(const vec2 &p_Size, const f32 p_Rotation) : Orthographic(vec2{0.f}, p_Size, p_Rotation)
{
}

Orthographic<2>::Orthographic(const vec2 &p_Position, const vec2 &p_Size, const f32 p_Rotation)
{
    Transform.Position = p_Position;
    Transform.Scale = p_Size;
    Transform.Rotation = p_Rotation;
}

void Orthographic<2>::UpdateMatrices() noexcept
{
    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;

    m_Projection = Transform.CameraTransform();
    m_InverseProjection = Transform.InverseCameraTransform();

    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
}

f32 Orthographic<2>::Size() const noexcept
{
    return Transform.Scale.y;
}

void Orthographic<2>::Size(const f32 p_Size) noexcept
{
    const f32 aspect = Transform.Scale.x / Transform.Scale.y;
    Transform.Scale = {aspect * p_Size, p_Size};
}

Orthographic<3>::Orthographic(const f32 p_Aspect, const f32 p_XYSize, const f32 p_Depth,
                              const mat3 &p_Rotation) noexcept
    : Orthographic(vec3{0.f}, p_Aspect, p_XYSize, p_Depth, p_Rotation)
{
}

Orthographic<3>::Orthographic(const vec3 &p_Position, const f32 p_Aspect, const f32 p_XYSize, const f32 p_Depth,
                              const mat3 &p_Rotation) noexcept
    : Orthographic(p_Position, {p_Aspect * p_XYSize, p_XYSize, p_Depth}, p_Rotation)
{
}

Orthographic<3>::Orthographic(const vec3 &p_Size, const mat3 &p_Rotation) noexcept
    : Orthographic(vec3{0.f}, p_Size, p_Rotation)
{
}

Orthographic<3>::Orthographic(const vec3 &p_Position, const vec3 &p_Size, const mat3 &p_Rotation) noexcept
{
    Transform.Position = p_Position;
    Transform.Scale = p_Size;
    Transform.Rotation = p_Rotation;
}

void Orthographic<3>::UpdateMatrices() noexcept
{
    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;

    m_Projection = Transform.CameraTransform();
    m_InverseProjection = Transform.InverseCameraTransform();

    if (m_YFlipped)
        Transform.Scale.y = -Transform.Scale.y;
}

f32 Orthographic<3>::Size() const
{
    return Transform.Scale.y;
}

void Orthographic<3>::Size(const f32 p_Size)
{
    const f32 aspect = Transform.Scale.x / Transform.Scale.y;
    Transform.Scale.x = aspect * p_Size;
    Transform.Scale.y = p_Size;
}

} // namespace ONYX