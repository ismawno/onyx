#include "utils/shapes.hpp"
#include "onyx/app/user_layer.hpp"
#include <imgui.h>

namespace Onyx::Demo
{
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Material(m_Material);
    p_Context->OutlineWidth(m_OutlineWidth);
    p_Context->Outline(m_OutlineColor);
    p_Context->Fill(m_Fill);
    p_Context->Outline(m_Outline);
    draw(p_Context);
}

template <Dimension D> void Shape<D>::Edit() noexcept
{
    ImGui::PushID(this);
    ImGui::Text("Transform");
    ImGui::SameLine();
    UserLayer::TransformEditor<D>(Transform, UserLayer::Flag_DisplayHelp);
    ImGui::Text("Material");
    ImGui::SameLine();
    UserLayer::MaterialEditor<D>(m_Material, UserLayer::Flag_DisplayHelp);
    ImGui::Checkbox("Fill", &m_Fill);
    ImGui::Checkbox("Outline", &m_Outline);
    ImGui::SliderFloat("Outline Width", &m_OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    if constexpr (D == D2)
        ImGui::ColorEdit4("Outline Color", m_OutlineColor.AsPointer());
    else
        ImGui::ColorEdit3("Outline Color", m_OutlineColor.AsPointer());
    ImGui::PopID();
}

template <Dimension D> static void dimensionEditor(fvec<D> &p_Dimensions) noexcept
{
    ImGui::PushID(&p_Dimensions);
    if constexpr (D == D2)
        ImGui::DragFloat2("Dimensions", glm::value_ptr(p_Dimensions), 0.01f, 0.f, FLT_MAX);
    else
        ImGui::DragFloat3("Dimensions", glm::value_ptr(p_Dimensions), 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

static void resolutionEditor(Resolution &p_Res) noexcept
{
    ImGui::PushID(&p_Res);
    ImGui::Combo("Resolution", reinterpret_cast<i32 *>(&p_Res), "Very Low\0Low\0Medium\0High\0Very High\0\0");
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
template <Dimension D> void Square<D>::Edit() noexcept
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
}

template <Dimension D> void Square<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Square(this->Transform.ComputeTransform(), m_Dimensions);
}

template <Dimension D> const char *Circle<D>::GetName() const noexcept
{
    return "Circle";
}

template <Dimension D> void Circle<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Circle(this->Transform.ComputeTransform(), m_Dimensions, m_Options);
}

template <Dimension D> void Circle<D>::Edit() noexcept
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::SliderFloat("Inner Fade", &m_Options.InnerFade, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Outer Fade", &m_Options.OuterFade, 0.f, 1.f, "%.2f");
    ImGui::SliderAngle("Lower Angle", &m_Options.LowerAngle);
    ImGui::SliderAngle("Upper Angle", &m_Options.UpperAngle);
    ImGui::SliderFloat("Hollowness", &m_Options.Hollowness, 0.f, 1.f, "%.2f");
    ImGui::PopID();
}

template <Dimension D> const char *NGon<D>::GetName() const noexcept
{
    return "NGon";
}

template <Dimension D> void NGon<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->NGon(this->Transform.ComputeTransform(), Sides, m_Dimensions);
}
template <Dimension D> void NGon<D>::Edit() noexcept
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::SliderInt("Sides", reinterpret_cast<i32 *>(&Sides), 3, ONYX_MAX_REGULAR_POLYGON_SIDES);
    ImGui::PopID();
}

template <Dimension D> const char *Polygon<D>::GetName() const noexcept
{
    return "Convex Polygon";
}
template <Dimension D> void Polygon<D>::Edit() noexcept
{
    Shape<D>::Edit();
    for (u32 i = 0; i < Vertices.size(); ++i)
    {
        ImGui::PushID(&Vertices[i]);
        if (Vertices.size() > 3 && ImGui::Button("X"))
        {
            Vertices.erase(Vertices.begin() + i);
            ImGui::PopID();
            break;
        }
        if (Vertices.size() > 3)
            ImGui::SameLine();

        ImGui::Text("Vertex %u: ", i);
        ImGui::SameLine();
        ImGui::DragFloat2("##Vertex", glm::value_ptr(Vertices[i]), 0.01f, -FLT_MAX, FLT_MAX);
        ImGui::PopID();
    }
}

template <Dimension D> void Polygon<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->ConvexPolygon(this->Transform.ComputeTransform(), Vertices);
}

template <Dimension D> const char *Stadium<D>::GetName() const noexcept
{
    return "Stadium";
}

template <Dimension D> void Stadium<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->Stadium(this->Transform.ComputeTransform(), m_Length, m_Diameter);
}

template <Dimension D> void Stadium<D>::Edit() noexcept
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

template <Dimension D> void RoundedSquare<D>::draw(RenderContext<D> *p_Context) noexcept
{
    p_Context->RoundedSquare(this->Transform.ComputeTransform(), m_Dimensions, m_Diameter);
}

template <Dimension D> const char *RoundedSquare<D>::GetName() const noexcept
{
    return "Rounded Square";
}

template <Dimension D> void RoundedSquare<D>::Edit() noexcept
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

const char *Cube::GetName() const noexcept
{
    return "Cube";
}

void Cube::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Cube(Transform.ComputeTransform());
}
void Cube::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
}

const char *Sphere::GetName() const noexcept
{
    return "Sphere";
}

void Sphere::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Sphere(Transform.ComputeTransform(), m_Dimensions, m_Res);
}
void Sphere::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    resolutionEditor(m_Res);
}

const char *Cylinder::GetName() const noexcept
{
    return "Cylinder";
}
void Cylinder::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    resolutionEditor(m_Res);
}

void Cylinder::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Cylinder(Transform.ComputeTransform(), m_Dimensions, m_Res);
}

const char *Capsule::GetName() const noexcept
{
    return "Capsule";
}

void Capsule::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->Capsule(Transform.ComputeTransform(), m_Length, m_Diameter, m_Res);
}

void Capsule::Edit() noexcept
{
    Shape<D3>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
    resolutionEditor(m_Res);
}

void RoundedCube::draw(RenderContext<D3> *p_Context) noexcept
{
    p_Context->RoundedCube(Transform.ComputeTransform(), m_Dimensions, m_Diameter, m_Res);
}

const char *RoundedCube::GetName() const noexcept
{
    return "Rounded Cube";
}

void RoundedCube::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
    resolutionEditor(m_Res);
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

} // namespace Onyx::Demo