#include "swdemo/shapes.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE void Shape<N>::Draw(RenderContext<N> *p_Context) noexcept
{
    Draw(p_Context, Transform.ComputeTransform());
}

ONYX_DIMENSION_TEMPLATE const char *Triangle<N>::GetName() const noexcept
{
    return "Triangle";
}

ONYX_DIMENSION_TEMPLATE void Triangle<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Triangle(p_Transform);
}

ONYX_DIMENSION_TEMPLATE const char *Rect<N>::GetName() const noexcept
{
    return "Rect";
}

ONYX_DIMENSION_TEMPLATE void Rect<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Rect(p_Transform);
}

ONYX_DIMENSION_TEMPLATE const char *Ellipse<N>::GetName() const noexcept
{
    return "Ellipse";
}

ONYX_DIMENSION_TEMPLATE void Ellipse<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Ellipse(p_Transform);
}

ONYX_DIMENSION_TEMPLATE const char *NGon<N>::GetName() const noexcept
{
    return "NGon";
}

ONYX_DIMENSION_TEMPLATE void NGon<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->NGon(Sides, p_Transform);
}

ONYX_DIMENSION_TEMPLATE const char *Polygon<N>::GetName() const noexcept
{
    return "Polygon";
}

ONYX_DIMENSION_TEMPLATE void Polygon<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Polygon(Vertices, p_Transform);
}

const char *Cuboid::GetName() const noexcept
{
    return "Cuboid";
}

void Cuboid::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Cuboid(p_Transform);
}

const char *Ellipsoid::GetName() const noexcept
{
    return "Ellipsoid";
}

void Ellipsoid::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Ellipsoid(p_Transform);
}

template class Shape<2>;
template class Shape<3>;

template class Triangle<2>;
template class Triangle<3>;

template class Rect<2>;
template class Rect<3>;

template class Ellipse<2>;
template class Ellipse<3>;

template class NGon<2>;
template class NGon<3>;

template class Polygon<2>;
template class Polygon<3>;

} // namespace ONYX