#include "swdemo/shapes.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
void Shape<N>::Draw(RenderContext<N> *p_Context) noexcept
{
    Draw(p_Context, Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
const char *Triangle<N>::GetName() const noexcept
{
    return "Triangle";
}

template <u32 N>
    requires(IsDim<N>())
void Triangle<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Triangle(p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
const char *Rect<N>::GetName() const noexcept
{
    return "Rect";
}

template <u32 N>
    requires(IsDim<N>())
void Rect<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Rect(p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
const char *Ellipse<N>::GetName() const noexcept
{
    return "Ellipse";
}

template <u32 N>
    requires(IsDim<N>())
void Ellipse<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Ellipse(p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
const char *NGon<N>::GetName() const noexcept
{
    return "NGon";
}

template <u32 N>
    requires(IsDim<N>())
void NGon<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->NGon(Sides, p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
const char *Polygon<N>::GetName() const noexcept
{
    return "Polygon";
}

template <u32 N>
    requires(IsDim<N>())
void Polygon<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Polygon(Vertices, p_Transform);
}

const char *Cuboid::GetName() const noexcept
{
    return "Cuboid";
}

void Cuboid::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Cuboid(p_Transform);
}

const char *Ellipsoid::GetName() const noexcept
{
    return "Ellipsoid";
}

void Ellipsoid::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Ellipsoid(p_Transform);
}

const char *Cylinder::GetName() const noexcept
{
    return "Cylinder";
}

void Cylinder::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Cylinder(p_Transform);
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