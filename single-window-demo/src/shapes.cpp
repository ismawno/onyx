#include "swdemo/shapes.hpp"
#include <imgui.h>

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
const char *Square<N>::GetName() const noexcept
{
    return "Square";
}

template <u32 N>
    requires(IsDim<N>())
void Square<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Square(p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
const char *Circle<N>::GetName() const noexcept
{
    return "Circle";
}

template <u32 N>
    requires(IsDim<N>())
void Circle<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Circle(LowerAngle, UpperAngle, p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
void Circle<N>::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::SliderAngle("Lower Angle", &LowerAngle);
    ImGui::SliderAngle("Upper Angle", &UpperAngle);
    ImGui::PopID();
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

template <u32 N>
    requires(IsDim<N>())
const char *Stadium<N>::GetName() const noexcept
{
    return "Stadium";
}

template <u32 N>
    requires(IsDim<N>())
void Stadium<N>::Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept
{
    p_Context->Material(this->Material);
    p_Context->Stadium(Length, Radius, p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
void Stadium<N>::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::SliderFloat("Length", &Length, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Radius", &Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::PopID();
}

const char *Cube::GetName() const noexcept
{
    return "Cube";
}

void Cube::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Cube(p_Transform);
}

const char *Sphere::GetName() const noexcept
{
    return "Sphere";
}

void Sphere::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Sphere(p_Transform);
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

const char *Capsule::GetName() const noexcept
{
    return "Capsule";
}

void Capsule::Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept
{
    p_Context->Material(Material);
    p_Context->Capsule(Length, Radius, p_Transform);
}

void Capsule::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::SliderFloat("Length", &Length, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Radius", &Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::PopID();
}

template class Shape<2>;
template class Shape<3>;

template class Triangle<2>;
template class Triangle<3>;

template class Square<2>;
template class Square<3>;

template class Circle<2>;
template class Circle<3>;

template class NGon<2>;
template class NGon<3>;

template class Polygon<2>;
template class Polygon<3>;

template class Stadium<2>;
template class Stadium<3>;

} // namespace ONYX