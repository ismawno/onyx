#include "sbox/shapes.hpp"
#include "onyx/app/user_layer.hpp"
#include "tkit/utils/dimension.hpp"
#include "onyx/core/imgui.hpp"

namespace Onyx::Demo
{
TKit::StaticArray16<NamedMesh<D2>> s_Meshes2{};
TKit::StaticArray16<NamedMesh<D3>> s_Meshes3{};
template <Dimension D> TKit::StaticArray16<NamedMesh<D>> &getMeshes()
{
    if constexpr (D == D2)
        return s_Meshes2;
    else
        return s_Meshes3;
}

template <Dimension D> const TKit::StaticArray16<NamedMesh<D>> &NamedMesh<D>::Get()
{
    return getMeshes<D>();
}

template <Dimension D> bool NamedMesh<D>::IsLoaded(const std::string_view p_Name)
{
    const auto &meshes = getMeshes<D>();
    for (const NamedMesh<D> &mesh : meshes)
        if (mesh.Name == p_Name)
            return true;
    return false;
}
template <Dimension D>
VKit::FormattedResult<> NamedMesh<D>::Load(const std::string_view p_Name, const std::string_view p_Path)
{
    const auto result = Onyx::Mesh<D>::Load(p_Path);
    if (!result)
        return VKit::FormattedResult<NamedMesh<D>>::Error(VKIT_FORMAT_ERROR(
            result.GetError().ErrorCode, "Failed to load mesh: '{}' - {}", p_Name, result.GetError().ToString()));

    Onyx::Mesh<D> mesh = result.GetValue();
    Onyx::Core::GetDeletionQueue().Push([mesh]() mutable { mesh.Destroy(); });
    const NamedMesh<D> nmesh{std::string(p_Name), mesh};

    auto &meshes = getMeshes<D>();
    meshes.Append(nmesh);
    return VKit::FormattedResult<>::Ok();
}
template <Dimension D> void Shape<D>::SetProperties(RenderContext<D> *p_Context)
{
    p_Context->Material(m_Material);
    p_Context->OutlineWidth(m_OutlineWidth);
    p_Context->Outline(m_OutlineColor);
    p_Context->Fill(m_Fill);
    p_Context->Outline(m_Outline);
}

template <Dimension D> void Shape<D>::DrawRaw(RenderContext<D> *p_Context) const
{
    draw(p_Context, this->Transform);
}
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context)
{
    SetProperties(p_Context);
    draw(p_Context, this->Transform);
}
template <Dimension D> void Shape<D>::DrawRaw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    draw(p_Context, p_Transform);
}
template <Dimension D> void Shape<D>::Draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform)
{
    SetProperties(p_Context);
    draw(p_Context, p_Transform);
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void Shape<D>::Edit()
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
    ImGui::ColorEdit4("Outline Color", m_OutlineColor.GetData());

    ImGui::PopID();
}

template <Dimension D> static void dimensionEditor(f32v<D> &p_Dimensions)
{
    ImGui::PushID(&p_Dimensions);
    if constexpr (D == D2)
        ImGui::DragFloat2("Dimensions", Math::AsPointer(p_Dimensions), 0.01f, 0.f, FLT_MAX);
    else
        ImGui::DragFloat3("Dimensions", Math::AsPointer(p_Dimensions), 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}
#endif

template <Dimension D> MeshShape<D>::MeshShape(const NamedMesh<D> &p_Mesh) : m_Mesh(p_Mesh)
{
}

template <Dimension D> const char *MeshShape<D>::GetName() const
{
    return m_Mesh.Name.c_str();
}
template <Dimension D> void MeshShape<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Mesh(p_Transform.ComputeTransform(), m_Mesh.Mesh, m_Dimensions);
}

template <Dimension D> const char *Triangle<D>::GetName() const
{
    return "Triangle";
}

template <Dimension D> void Triangle<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Triangle(p_Transform.ComputeTransform());
}

template <Dimension D> const char *Square<D>::GetName() const
{
    return "Square";
}
#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void Square<D>::Edit()
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
}
#endif

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void MeshShape<D>::Edit()
{
    Shape<D>::Edit();
    dimensionEditor<D>(m_Dimensions);
}
#endif
template <Dimension D> void Square<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Square(p_Transform.ComputeTransform(), m_Dimensions);
}

template <Dimension D> const char *Circle<D>::GetName() const
{
    return "Circle";
}

template <Dimension D> void Circle<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Circle(p_Transform.ComputeTransform(), m_Dimensions, m_Options);
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void Circle<D>::Edit()
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
#endif

template <Dimension D> const char *NGon<D>::GetName() const
{
    return "NGon";
}

template <Dimension D> void NGon<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->NGon(p_Transform.ComputeTransform(), Sides, m_Dimensions);
}
#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void NGon<D>::Edit()
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
    ImGui::PushID(this);
    const u32 mn = 3;
    const u32 mx = ONYX_MAX_REGULAR_POLYGON_SIDES;
    ImGui::SliderScalar("Sides", ImGuiDataType_U32, &Sides, &mn, &mx);
    ImGui::PopID();
}
#endif

template <Dimension D> const char *Polygon<D>::GetName() const
{
    return "Polygon";
}
#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void Polygon<D>::Edit()
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
        ImGui::DragFloat2("##Vertex", Math::AsPointer(Vertices[i]), 0.01f, -FLT_MAX, FLT_MAX);
        ImGui::PopID();
    }
}
#endif

template <Dimension D> void Polygon<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Polygon(p_Transform.ComputeTransform(), Vertices);
}

template <Dimension D> const char *Stadium<D>::GetName() const
{
    return "Stadium";
}

template <Dimension D> void Stadium<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->Stadium(p_Transform.ComputeTransform(), m_Length, m_Diameter);
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void Stadium<D>::Edit()
{
    Shape<D>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}
#endif

template <Dimension D>
void RoundedSquare<D>::draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const
{
    p_Context->RoundedSquare(p_Transform.ComputeTransform(), m_Dimensions, m_Diameter);
}

template <Dimension D> const char *RoundedSquare<D>::GetName() const
{
    return "Rounded Square";
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void RoundedSquare<D>::Edit()
{
    Shape<D>::Edit();
    dimensionEditor<D2>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
}
#endif

const char *Cube::GetName() const
{
    return "Cube";
}

void Cube::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const
{
    p_Context->Cube(p_Transform.ComputeTransform(), m_Dimensions);
}
#ifdef ONYX_ENABLE_IMGUI
void Cube::Edit()
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
}
#endif

const char *Sphere::GetName() const
{
    return "Sphere";
}

void Sphere::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const
{
    p_Context->Sphere(p_Transform.ComputeTransform(), m_Dimensions, m_Res);
}
#ifdef ONYX_ENABLE_IMGUI
void Sphere::Edit()
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}
#endif

const char *Cylinder::GetName() const
{
    return "Cylinder";
}
#ifdef ONYX_ENABLE_IMGUI
void Cylinder::Edit()
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}
#endif

void Cylinder::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const
{
    p_Context->Cylinder(p_Transform.ComputeTransform(), m_Dimensions, m_Res);
}

const char *Capsule::GetName() const
{
    return "Capsule";
}

void Capsule::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const
{
    p_Context->Capsule(p_Transform.ComputeTransform(), m_Length, m_Diameter, m_Res);
}

#ifdef ONYX_ENABLE_IMGUI
void Capsule::Edit()
{
    Shape<D3>::Edit();
    ImGui::PushID(this);
    ImGui::DragFloat("Length", &m_Length, 0.01f, 0.f, FLT_MAX);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}
#endif

void RoundedCube::draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const
{
    p_Context->RoundedCube(p_Transform.ComputeTransform(), m_Dimensions, m_Diameter, m_Res);
}

const char *RoundedCube::GetName() const
{
    return "Rounded Cube";
}

#ifdef ONYX_ENABLE_IMGUI
void RoundedCube::Edit()
{
    Shape<D3>::Edit();
    dimensionEditor<D3>(m_Dimensions);
    ImGui::PushID(this);
    ImGui::DragFloat("Diameter", &m_Diameter, 0.01f, 0.f, FLT_MAX);
    ImGui::PopID();
    UserLayer::ResolutionEditor("Resolution", m_Res, UserLayer::Flag_DisplayHelp);
}
#endif

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
