#include "utils/shapes.hpp"
#include <imgui.h>

namespace Onyx
{
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Material(Material);
    p_Context->OutlineWidth(OutlineWidth);
    p_Context->Outline(OutlineColor);
    p_Context->Fill(Fill);
    p_Context->Outline(Outline);
    draw(p_Context);
}

template <Dimension D> void Shape<D>::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::Checkbox("Fill", &Fill);
    ImGui::Checkbox("Outline", &Outline);
    ImGui::SliderFloat("Outline Width", &OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    if constexpr (D == D2)
        ImGui::ColorEdit4("Outline Color", OutlineColor.AsPointer());
    else
        ImGui::ColorEdit3("Outline Color", OutlineColor.AsPointer());
    ImGui::PopID();
}

template <Dimension D> const char *Triangle<D>::GetName() const noexcept
{
    return "Triangle";
}

template <Dimension D> void Triangle<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Triangle(this->Transform.ComputeTransform());
}

template <Dimension D> const char *Square<D>::GetName() const noexcept
{
    return "Square";
}

template <Dimension D> void Square<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Square(this->Transform.ComputeTransform());
}

template <Dimension D> const char *Circle<D>::GetName() const noexcept
{
    return "Circle";
}

template <Dimension D> void Circle<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Arc(this->Transform.ComputeTransform(), LowerAngle, UpperAngle, InnerFade, LowerFade, Hollowness);
}

template <Dimension D> void Circle<D>::Edit() noexcept
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::SliderFloat("Inner Fade", &InnerFade, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Outer Fade", &LowerFade, 0.f, 1.f, "%.2f");
    ImGui::SliderAngle("Lower Angle", &LowerAngle);
    ImGui::SliderAngle("Upper Angle", &UpperAngle);
    ImGui::SliderFloat("Hollowness", &Hollowness, 0.f, 1.f, "%.2f");
    ImGui::PopID();
}

template <Dimension D> const char *NGon<D>::GetName() const noexcept
{
    return "NGon";
}

template <Dimension D> void NGon<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->NGon(this->Transform.ComputeTransform(), Sides);
}

template <Dimension D> const char *Polygon<D>::GetName() const noexcept
{
    return "Polygon";
}

template <Dimension D> void Polygon<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Polygon(this->Transform.ComputeTransform(), Vertices);
}

template <Dimension D> const char *Stadium<D>::GetName() const noexcept
{
    return "Stadium";
}

template <Dimension D> void Stadium<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Stadium(this->Transform.ComputeTransform(), Length, Radius);
}

template <Dimension D> void Stadium<D>::Edit() noexcept
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Radius", &Radius, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

template <Dimension D> void RoundedSquare<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->RoundedSquare(this->Transform.ComputeTransform(), Dimensions, Radius);
}

template <Dimension D> const char *RoundedSquare<D>::GetName() const noexcept
{
    return "Rounded Square";
}

template <Dimension D> void RoundedSquare<D>::Edit() noexcept
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat2("Dimensions", glm::value_ptr(Dimensions), 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Radius", &Radius, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

const char *Cube::GetName() const noexcept
{
    return "Cube";
}

void Cube::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Cube(this->Transform.ComputeTransform());
}

const char *Sphere::GetName() const noexcept
{
    return "Sphere";
}

void Sphere::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Sphere(this->Transform.ComputeTransform());
}

const char *Cylinder::GetName() const noexcept
{
    return "Cylinder";
}

void Cylinder::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Cylinder(this->Transform.ComputeTransform());
}

const char *Capsule::GetName() const noexcept
{
    return "Capsule";
}

void Capsule::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Capsule(Transform.ComputeTransform(), Length, Radius);
}

void Capsule::Edit() noexcept
{
    Shape<D3>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Radius", &Radius, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

void RoundedCube::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->RoundedCube(Transform.ComputeTransform(), Dimensions, Radius);
}

const char *RoundedCube::GetName() const noexcept
{
    return "Rounded Cube";
}

void RoundedCube::Edit() noexcept
{
    Shape<D3>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat3("Dimensions", glm::value_ptr(Dimensions), 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Radius", &Radius, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

template class Shape<D2>;
template class Shape<D3>;

template class Triangle<D2>;
template class Triangle<D3>;

template class Square<D2>;
template class Square<D3>;

template class Circle<D2>;
template class Circle<D3>;

template class NGon<D2>;
template class NGon<D3>;

template class Polygon<D2>;
template class Polygon<D3>;

template class Stadium<D2>;
template class Stadium<D3>;

template class RoundedSquare<D2>;
template class RoundedSquare<D3>;

} // namespace Onyx