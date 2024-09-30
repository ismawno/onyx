#include "core/pch.hpp"
#include "onyx/draw/primitives/ellipsoid.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
Ellipsoid3D::Ellipsoid3D(const f32 p_Radius1, const f32 p_Radius2, const f32 p_Radius3, const Color &p_Color) noexcept
    : ModelShape3D(Model::GetSphere(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    Transform.Scale.x = p_Radius1;
    Transform.Scale.y = p_Radius2;
    Transform.Scale.z = p_Radius3;
}

Ellipsoid3D::Ellipsoid3D(const f32 p_Radius, const Color &p_Color) noexcept
    : Ellipsoid3D(p_Radius, p_Radius, p_Radius, p_Color)
{
}

Ellipsoid3D::Ellipsoid3D(const Color &p_Color) noexcept : Ellipsoid3D(1.f, 1.f, 1.f, p_Color)
{
}

f32 Ellipsoid3D::GetRadius1() const noexcept
{
    return Transform.Scale.x;
}

f32 Ellipsoid3D::GetRadius2() const noexcept
{
    return Transform.Scale.y;
}

f32 Ellipsoid3D::GetRadius3() const noexcept
{
    return Transform.Scale.z;
}

f32 Ellipsoid3D::GetRadius() const noexcept
{
    return 0.5f * (Transform.Scale.x + Transform.Scale.y);
}

void Ellipsoid3D::SetRadius1(const f32 p_Radius1) noexcept
{
    Transform.Scale.x = p_Radius1;
}

void Ellipsoid3D::SetRadius2(const f32 p_Radius2) noexcept
{
    Transform.Scale.y = p_Radius2;
}

void Ellipsoid3D::SetRadius3(const f32 p_Radius3) noexcept
{
    Transform.Scale.z = p_Radius3;
}

void Ellipsoid3D::SetRadius(const f32 p_Radius) noexcept
{
    Transform.Scale.x = p_Radius;
    Transform.Scale.y = p_Radius;
}
} // namespace ONYX