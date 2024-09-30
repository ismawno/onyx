#include "core/pch.hpp"
#include "onyx/draw/primitives/ellipse.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Ellipse<N>::Ellipse(const f32 p_Radius1, const f32 p_Radius2, const Color &p_Color) noexcept
    : ModelShape<N>(Model::GetCircle<N>(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    this->Transform.Scale.x = p_Radius1;
    this->Transform.Scale.y = p_Radius2;
}

ONYX_DIMENSION_TEMPLATE Ellipse<N>::Ellipse(const f32 p_Radius, const Color &p_Color) noexcept
    : Ellipse(p_Radius, p_Radius, p_Color)
{
}

ONYX_DIMENSION_TEMPLATE Ellipse<N>::Ellipse(const Color &p_Color) noexcept : Ellipse(1.f, 1.f, p_Color)
{
}

ONYX_DIMENSION_TEMPLATE f32 Ellipse<N>::GetRadius1() const noexcept
{
    return this->Transform.Scale.x;
}

ONYX_DIMENSION_TEMPLATE f32 Ellipse<N>::GetRadius2() const noexcept
{
    return this->Transform.Scale.y;
}

ONYX_DIMENSION_TEMPLATE f32 Ellipse<N>::GetRadius() const noexcept
{
    return 0.5f * (this->Transform.Scale.x + this->Transform.Scale.y);
}

ONYX_DIMENSION_TEMPLATE void Ellipse<N>::SetRadius1(const f32 p_Radius1) noexcept
{
    this->Transform.Scale.x = p_Radius1;
}

ONYX_DIMENSION_TEMPLATE void Ellipse<N>::SetRadius2(const f32 p_Radius2) noexcept
{
    this->Transform.Scale.y = p_Radius2;
}

ONYX_DIMENSION_TEMPLATE void Ellipse<N>::SetRadius(const f32 p_Radius) noexcept
{
    this->Transform.Scale.x = p_Radius;
    this->Transform.Scale.y = p_Radius;
}

template class Ellipse<2>;
template class Ellipse<3>;
} // namespace ONYX