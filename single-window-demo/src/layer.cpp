#include "swdemo/layer.hpp"
#include "onyx/app/mwapp.hpp"
#include <imgui.h>
#include <implot.h>

namespace ONYX
{

SWExampleLayer::SWExampleLayer(Application *p_Application) noexcept : Layer("Example"), m_Application(p_Application)
{
}

void SWExampleLayer::OnStart() noexcept
{
    m_Context2 = m_Application->GetMainWindow()->GetRenderContext2D();
    m_Context3 = m_Application->GetMainWindow()->GetRenderContext3D();
    m_Context3->AddLight({1.f, 0.f, 0.f}, 0.5f);
}

ONYX_DIMENSION_TEMPLATE void SWExampleLayer::drawShapes(RenderContext<N> *p_Context,
                                                        const DynamicArray<KIT::Scope<Shape<N>>> &p_Shapes,
                                                        const Transform<N> &p_Axes) noexcept
{
    p_Context->Background(m_BackgroundColor);
    p_Context->KeepWindowAspect();

    if constexpr (N == 3)
    {
        if (m_Perspective)
            p_Context->Perspective(m_FieldOfView, m_Near, m_Far);
        else
            p_Context->Orthographic();
    }

    p_Context->TransformAxes(p_Axes.Translation, p_Axes.Scale, p_Axes.Rotation);

    p_Context->Fill(m_ShapeColor);
    for (const auto &shape : p_Shapes)
        shape->Draw(p_Context);
}

void SWExampleLayer::OnRender() noexcept
{
    drawShapes(m_Context2, m_Shapes2, m_Axes2);
    drawShapes(m_Context3, m_Shapes3, m_Axes3);
}

ONYX_DIMENSION_TEMPLATE static void editTransform(Transform<N> &p_Transform) noexcept
{
    ImGui::PushID(&p_Transform);
    if constexpr (N == 2)
    {
        ImGui::DragFloat2("Translation", glm::value_ptr(p_Transform.Translation), 0.03f);
        ImGui::DragFloat2("Scale", glm::value_ptr(p_Transform.Scale), 0.03f);
        ImGui::DragFloat("Rotation", &p_Transform.Rotation, 0.03f);
    }
    else
    {
        ImGui::DragFloat3("Translation", glm::value_ptr(p_Transform.Translation), 0.03f);
        ImGui::DragFloat3("Scale", glm::value_ptr(p_Transform.Scale), 0.03f);

        vec3 angles{0.f};
        if (ImGui::DragFloat3("Rotate (global)", glm::value_ptr(angles), 0.03f, 0.f, 0.f, "Slide!"))
            p_Transform.Rotation = glm::normalize(glm::quat(angles) * p_Transform.Rotation);

        ImGui::Spacing();

        if (ImGui::DragFloat3("Rotate (Local)", glm::value_ptr(angles), 0.03f, 0.f, 0.f, "Slide!"))
            p_Transform.Rotation = glm::normalize(p_Transform.Rotation * glm::quat(angles));
        if (ImGui::Button("Reset rotation"))
            p_Transform.Rotation = quat{1.f, 0.f, 0.f, 0.f};
    }
    ImGui::PopID();
}

ONYX_DIMENSION_TEMPLATE void SWExampleLayer::renderShapeSpawn(DynamicArray<KIT::Scope<Shape<N>>> &p_Shapes) noexcept
{
    static i32 toSpawn = 0;
    static DynamicArray<vec<N>> vertices;
    static i32 sides = 3;

    if constexpr (N == 2)
        ImGui::Combo("Shape", &toSpawn, "Triangle\0Rectangle\0Ellipse\0NGon\0Polygon\0\0");
    else
        ImGui::Combo("Shape", &toSpawn, "Triangle\0Rectangle\0Ellipse\0NGon\0Polygon\0Cuboid\0Sphere\0\0");

    if (toSpawn == 3)
        ImGui::SliderInt("Sides", &sides, 3, ONYX_MAX_REGULAR_POLYGON_SIDES);

    if (toSpawn == 4)
    {
        static vec<N> toAdd{0.f};
        if constexpr (N == 2)
            ImGui::DragFloat2("Vertex", glm::value_ptr(toAdd), 0.1f);
        else
            ImGui::DragFloat3("Vertex", glm::value_ptr(toAdd), 0.1f);
        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            vertices.push_back(toAdd);
            toAdd = vec<N>{0.f};
        }
        for (usize i = 0; i < vertices.size(); ++i)
        {
            if (ImGui::Button("X"))
            {
                vertices.erase(vertices.begin() + i);
                break;
            }
            ImGui::SameLine();
            if constexpr (N == 2)
                ImGui::Text("Vertex %zu: (%.2f, %.2f)", i, vertices[i].x, vertices[i].y);
            else
                ImGui::Text("Vertex %zu: (%.2f, %.2f, %.2f)", i, vertices[i].x, vertices[i].y, vertices[i].z);
        }
    }

    if (ImGui::Button(N == 2 ? "Spawn2D" : "Spawn3D"))
    {
        if (toSpawn == 0)
            p_Shapes.push_back(KIT::Scope<Triangle<N>>::Create());
        else if (toSpawn == 1)
            p_Shapes.push_back(KIT::Scope<Rect<N>>::Create());
        else if (toSpawn == 2)
            p_Shapes.push_back(KIT::Scope<Ellipse<N>>::Create());
        else if (toSpawn == 3)
        {
            auto ngon = KIT::Scope<NGon<N>>::Create();
            ngon->Sides = static_cast<u32>(sides);
            p_Shapes.push_back(std::move(ngon));
        }
        else if (toSpawn == 4)
        {
            auto polygon = KIT::Scope<Polygon<N>>::Create();
            polygon->Vertices = vertices;
            p_Shapes.push_back(std::move(polygon));
        }
        else if constexpr (N == 3)
        {
            if (toSpawn == 5)
                p_Shapes.push_back(KIT::Scope<Cuboid>::Create());
            else if (toSpawn == 6)
                p_Shapes.push_back(KIT::Scope<Ellipsoid>::Create());
        }
    }

    if (ImGui::Button("Clear"))
        p_Shapes.clear();

    static usize selected = 0;
    const usize size = static_cast<usize>(p_Shapes.size());
    for (usize i = 0; i < size; ++i)
    {
        if (ImGui::Button("X"))
        {
            p_Shapes.erase(p_Shapes.begin() + i);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable(p_Shapes[i]->GetName(), selected == i))
            selected = i;
    }
    ImGui::Text("Selected transform");
    if (selected < size)
        editTransform(p_Shapes[selected]->Transform);
}

void SWExampleLayer::OnImGuiRender() noexcept
{
    if (ImGui::Begin("Shapes"))
    {
        const vec2 mpos2 = m_Context2->GetMouseCoordinates();
        const vec3 mpos3 = m_Context3->GetMouseCoordinates(0.f);
        // const vec2 mpos = Input::GetMousePosition(m_Application->GetMainWindow());
        ImGui::Text("Mouse Position 2D: (%.2f, %.2f)", mpos2.x, mpos2.y);
        ImGui::Text("Mouse Position 3D: (%.2f, %.2f, %.2f)", mpos3.x, mpos3.y, mpos3.z);

        if (ImGui::CollapsingHeader("Background color"))
            ImGui::ColorPicker3("Background", m_BackgroundColor.AsPointer());
        if (ImGui::CollapsingHeader("Shape color"))
            ImGui::ColorPicker3("Shape", m_ShapeColor.AsPointer());

        if (ImGui::CollapsingHeader("Axes"))
        {
            ImGui::Text("2D Axes");
            editTransform(m_Axes2);

            ImGui::Text("3D Axes");
            editTransform(m_Axes3);

            ImGui::Checkbox("Perspective", &m_Perspective);
            if (m_Perspective)
            {
                static f32 rads = 50.f;
                if (ImGui::SliderFloat("Field of view", &rads, 50.f, 90.f))
                    m_FieldOfView = glm::radians(rads);
                ImGui::SliderFloat("Near", &m_Near, 0.1f, 10.f);
                ImGui::SliderFloat("Far", &m_Far, 10.f, 100.f);
            }
        }
        if (ImGui::CollapsingHeader("2D Shapes"))
            renderShapeSpawn(m_Shapes2);
        if (ImGui::CollapsingHeader("3D Shapes"))
            renderShapeSpawn(m_Shapes3);
    }
    ImGui::End();
}

} // namespace ONYX