#include "swdemo/shapes.hpp"
#include <imgui.h>

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
void Shape<N>::Draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Material(Material);
    p_Context->Fill(Fill);
    p_Context->Outline(Outline);
    p_Context->OutlineWidth(OutlineWidth);
    p_Context->Outline(OutlineColor);
    draw(p_Context);
}

template <u32 N>
    requires(IsDim<N>())
void Shape<N>::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::Checkbox("Fill", &Fill);
    ImGui::Checkbox("Outline", &Outline);
    ImGui::SliderFloat("Outline Width", &OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    if constexpr (N == 2)
        ImGui::ColorEdit4("Outline Color", OutlineColor.AsPointer());
    else
        ImGui::ColorEdit3("Outline Color", OutlineColor.AsPointer());
    ImGui::PopID();
}

template <u32 N>
    requires(IsDim<N>())
const char *Triangle<N>::GetName() const noexcept
{
    return "Triangle";
}

template <u32 N>
    requires(IsDim<N>())
void Triangle<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Triangle(this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
const char *Square<N>::GetName() const noexcept
{
    return "Square";
}

template <u32 N>
    requires(IsDim<N>())
void Square<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Square(this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
const char *Circle<N>::GetName() const noexcept
{
    return "Circle";
}

template <u32 N>
    requires(IsDim<N>())
void Circle<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Circle(LowerAngle, UpperAngle, Hollowness, this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
void Circle<N>::Edit() noexcept
{
    Shape<N>::Edit();
    ImGui::PushID(this);
    ImGui::SliderAngle("Lower Angle", &LowerAngle);
    ImGui::SliderAngle("Upper Angle", &UpperAngle);
    ImGui::SliderFloat("Hollowness", &Hollowness, 0.f, 1.f, "%.2f");
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
void NGon<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->NGon(Sides, this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
const char *Polygon<N>::GetName() const noexcept
{
    return "Polygon";
}

template <u32 N>
    requires(IsDim<N>())
void Polygon<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Polygon(Vertices, this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
const char *Stadium<N>::GetName() const noexcept
{
    return "Stadium";
}

template <u32 N>
    requires(IsDim<N>())
void Stadium<N>::draw(RenderContext<N> *p_Context) noexcept
{
    p_Context->Stadium(Length, Radius, this->Transform.ComputeTransform());
}

template <u32 N>
    requires(IsDim<N>())
void Stadium<N>::Edit() noexcept
{
    Shape<N>::Edit();
    ImGui::PushID(this);
    ImGui::SliderFloat("Length", &Length, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Radius", &Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::PopID();
}

const char *Cube::GetName() const noexcept
{
    return "Cube";
}

void Cube::draw(RenderContext3D *p_Context) noexcept
{
    p_Context->Cube(this->Transform.ComputeTransform());
}

const char *Sphere::GetName() const noexcept
{
    return "Sphere";
}

void Sphere::draw(RenderContext3D *p_Context) noexcept
{
    p_Context->Sphere(this->Transform.ComputeTransform());
}

const char *Cylinder::GetName() const noexcept
{
    return "Cylinder";
}

void Cylinder::draw(RenderContext3D *p_Context) noexcept
{
    p_Context->Cylinder(this->Transform.ComputeTransform());
}

const char *Capsule::GetName() const noexcept
{
    return "Capsule";
}

void Capsule::draw(RenderContext3D *p_Context) noexcept
{
    p_Context->Capsule(Length, Radius, Transform.ComputeTransform());
}

void Capsule::Edit() noexcept
{
    Shape3D::Edit();
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