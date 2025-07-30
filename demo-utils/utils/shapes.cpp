#include "utils/shapes.hpp"
#include "onyx/app/user_layer.hpp"
#include "tkit/utils/dimension.hpp"
#include <imgui.h>
#include <filesystem>

namespace Onyx::Demo
{

namespace fs = std::filesystem;

TKit::StaticArray16<NamedMesh<D2>> s_Meshes2{};
TKit::StaticArray16<NamedMesh<D3>> s_Meshes3{};
template <Dimension D> TKit::StaticArray16<NamedMesh<D>> &getMeshes() noexcept
{
    if constexpr (D == D2)
        return s_Meshes2;
    else
        return s_Meshes3;
}

template <Dimension D> const TKit::StaticArray16<NamedMesh<D>> &NamedMesh<D>::Get() noexcept
{
    return getMeshes<D>();
}

template <Dimension D> TKit::StaticArray16<std::string> NamedMesh<D>::Query(const std::string_view p_Directory) noexcept
{
    TKit::StaticArray16<std::string> names;
    if (!fs::exists(p_Directory))
        return names;

    for (const auto &entry : fs::directory_iterator(p_Directory))
    {
        const auto &path = entry.path();
        const std::string name = path.filename().string();
        names.Append(name);
    }
    return names;
}

template <Dimension D> bool NamedMesh<D>::IsLoaded(const std::string_view p_Name) noexcept
{
    const auto &meshes = getMeshes<D>();
    for (const NamedMesh<D> &mesh : meshes)
        if (mesh.Name == p_Name)
            return true;
    return false;
}
template <Dimension D>
VKit::FormattedResult<NamedMesh<D>> NamedMesh<D>::Load(const std::string_view p_Name, const std::string_view p_Path,
                                                       const fmat<D> &p_Transform) noexcept
{
    const auto result = Onyx::Mesh<D>::Load(p_Path, &p_Transform);
    if (!result)
        return VKit::FormattedResult<NamedMesh<D>>::Error(VKIT_FORMAT_ERROR(
            result.GetError().ErrorCode, "Failed to load mesh: '{}' - {}", p_Name, result.GetError().ToString()));

    Onyx::Mesh<D> mesh = result.GetValue();
    Onyx::Core::GetDeletionQueue().Push([mesh]() mutable { mesh.Destroy(); });
    const NamedMesh<D> nmesh{std::string(p_Name), mesh};

    auto &meshes = getMeshes<D>();
    meshes.Append(nmesh);
    return VKit::FormattedResult<NamedMesh<D>>::Ok(nmesh);
}
template <Dimension D> void Shape<D>::SetProperties(RenderContext<D> *p_Context) noexcept
{
    p_Context->Material(m_Material);
    p_Context->OutlineWidth(m_OutlineWidth);
    p_Context->Outline(m_OutlineColor);
    p_Context->Fill(m_Fill);
    p_Context->Outline(m_Outline);
}

template <Dimension D> void Shape<D>::DrawRaw(RenderContext<D> *p_Context) const noexcept
{
    draw(p_Context, this->Transform);
}
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context) noexcept
{
    SetProperties(p_Context);
    draw(p_Context, this->Transform);
}
template <Dimension D>
void Shape<D>::DrawRaw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    draw(p_Context, p_Transform);
}
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) noexcept
{
    SetProperties(p_Context);
    draw(p_Context, p_Transform);
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
    ImGui::ColorEdit4("Outline Color", m_OutlineColor.AsPointer());

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

template <Dimension D> MeshShape<D>::MeshShape(const NamedMesh<D> &p_Mesh) noexcept : m_Mesh(p_Mesh)
{
}

template <Dimension D> const char *MeshShape<D>::GetName() const noexcept
{
    return m_Mesh.Name.c_str();
}
template <Dimension D>
void MeshShape<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Mesh(p_Transform.ComputeTransform(), m_Mesh.Mesh, m_Dimensions);
}

template <Dimension D> const char *Triangle<D>::GetName() const noexcept
{
    return "Triangle";
}

template <Dimension D>
void Triangle<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Triangle(p_Transform.ComputeTransform());
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

template <Dimension D> void MeshShape<D>::Edit() noexcept
{
    Shape<D>::Edit();
    dimensionEditor<D>(m_Dimensions);
}
template <Dimension D>
void Square<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Square(p_Transform.ComputeTransform(), m_Dimensions);
}

template <Dimension D> const char *Circle<D>::GetName() const noexcept
{
    return "Circle";
}

template <Dimension D>
void Circle<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Circle(p_Transform.ComputeTransform(), m_Dimensions, m_Options);
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

template <Dimension D>
void NGon<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->NGon(p_Transform.ComputeTransform(), Sides, m_Dimensions);
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
    return "Polygon";
}
template <Dimension D> void Polygon<D>::Edit() noexcept
{
    Shape<D>::Edit();
    for (u32 i = 0; i < Vertices.GetSize(); ++i)
    {
        ImGui::PushID(&Vertices[i]);
        if (Vertices.GetSize() > 3 && ImGui::Button("X"))
        {
            Vertices.RemoveOrdered(Vertices.begin() + i);
            ImGui::PopID();
            break;
        }
        if (Vertices.GetSize() > 3)
            ImGui::SameLine();

        ImGui::Text("Vertex %u: ", i);
        ImGui::SameLine();
        ImGui::DragFloat2("##Vertex", glm::value_ptr(Vertices[i]), 0.01f, -FLT_MAX, FLT_MAX);
        ImGui::PopID();
    }
}

template <Dimension D>
void Polygon<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Polygon(p_Transform.ComputeTransform(), Vertices);
}

template <Dimension D> const char *Stadium<D>::GetName() const noexcept
{
    return "Stadium";
}

template <Dimension D>
void Stadium<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->Stadium(p_Transform.ComputeTransform(), m_Length, m_Diameter);
}

template <Dimension D> void Stadium<D>::Edit() noexcept
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}

template <Dimension D>
void RoundedSquare<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept
{
    p_Context->RoundedSquare(p_Transform.ComputeTransform(), m_Dimensions, m_Diameter);
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

void Cube::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept
{
    p_Context->Cube(p_Transform.ComputeTransform());
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

void Sphere::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept
{
    p_Context->Sphere(p_Transform.ComputeTransform(), m_Dimensions, m_Res);
}
void Sphere::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}

const char *Cylinder::GetName() const noexcept
{
    return "Cylinder";
}
void Cylinder::Edit() noexcept
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}

void Cylinder::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept
{
    p_Context->Cylinder(p_Transform.ComputeTransform(), m_Dimensions, m_Res);
}

const char *Capsule::GetName() const noexcept
{
    return "Capsule";
}

void Capsule::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept
{
    p_Context->Capsule(p_Transform.ComputeTransform(), m_Length, m_Diameter, m_Res);
}

void Capsule::Edit() noexcept
{
    Shape<D3>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}

void RoundedCube::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept
{
    p_Context->RoundedCube(p_Transform.ComputeTransform(), m_Dimensions, m_Diameter, m_Res);
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
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}

template struct ONYX_API NamedMesh<D2>;
template struct ONYX_API NamedMesh<D3>;

template class ONYX_API Shape<D2>;
template class ONYX_API Shape<D3>;

template class ONYX_API MeshShape<D2>;
template class ONYX_API MeshShape<D3>;

template class ONYX_API Triangle<D2>;
template class ONYX_API Triangle<D3>;

template class ONYX_API Square<D2>;
template class ONYX_API Square<D3>;

template class ONYX_API Circle<D2>;
template class ONYX_API Circle<D3>;

template class ONYX_API NGon<D2>;
template class ONYX_API NGon<D3>;

template class ONYX_API Polygon<D2>;
template class ONYX_API Polygon<D3>;

template class ONYX_API Stadium<D2>;
template class ONYX_API Stadium<D3>;

template class ONYX_API RoundedSquare<D2>;
template class ONYX_API RoundedSquare<D3>;

} // namespace Onyx::Demo
