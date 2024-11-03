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
    m_LayerData2.Context = m_Application->GetMainWindow()->GetRenderContext<D2>();
    m_LayerData3.Context = m_Application->GetMainWindow()->GetRenderContext<D3>();
}

template <Dimension D> static void editMaterial(MaterialData<D> &p_Material) noexcept
{
    if constexpr (D == D2)
        ImGui::ColorEdit4("Color", p_Material.Color.AsPointer());
    else
    {
        if (ImGui::SliderFloat("Diffuse contribution", &p_Material.DiffuseContribution, 0.f, 1.f))
            p_Material.SpecularContribution = 1.f - p_Material.DiffuseContribution;
        if (ImGui::SliderFloat("Specular contribution", &p_Material.SpecularContribution, 0.f, 1.f))
            p_Material.DiffuseContribution = 1.f - p_Material.SpecularContribution;
        ImGui::SliderFloat("Specular sharpness", &p_Material.SpecularSharpness, 0.f, 512.f, "%.2f",
                           ImGuiSliderFlags_Logarithmic);

        ImGui::ColorEdit3("Color", p_Material.Color.AsPointer());
    }
}

template <Dimension D> void SWExampleLayer::drawShapes(const LayerData<D> &p_Data) noexcept
{
    p_Data.Context->Flush(m_BackgroundColor);
    p_Data.Context->KeepWindowAspect();

    if constexpr (D == D3)
    {
        if (m_Perspective)
            p_Data.Context->Perspective(m_FieldOfView, m_Near, m_Far);
        else
            p_Data.Context->Orthographic();
    }

    p_Data.Context->TransformAxes(p_Data.AxesTransform);

    for (const auto &shape : p_Data.Shapes)
        shape->Draw(p_Data.Context);

    p_Data.Context->Outline(false);
    if (p_Data.DrawAxes)
    {
        p_Data.Context->Material(p_Data.AxesMaterial);
        p_Data.Context->Fill();
        p_Data.Context->Axes(p_Data.AxesThickness);
    }

    for (const auto &vertex : p_Data.PolygonVertices)
    {
        p_Data.Context->Push();
        p_Data.Context->Scale(0.02f);
        p_Data.Context->Translate(vertex);
        p_Data.Context->Circle();
        p_Data.Context->Pop();
    }

    if constexpr (D == D3)
    {
        p_Data.Context->AmbientColor(m_Ambient);
        for (const auto &light : m_DirectionalLights)
        {
            p_Data.Context->LightColor(light.Color);
            p_Data.Context->DirectionalLight(light);
        }
        for (const auto &light : m_PointLights)
        {
            if (m_DrawLights)
            {
                p_Data.Context->Push();
                p_Data.Context->Fill(light.Color);
                p_Data.Context->Scale(0.01f);
                p_Data.Context->Translate(light.PositionAndIntensity);
                p_Data.Context->Sphere();
                p_Data.Context->Pop();
            }
            p_Data.Context->LightColor(light.Color);
            p_Data.Context->PointLight(light);
        }
    }
}

template <Dimension D> static void editTransform(Transform<D> &p_Transform) noexcept
{
    ImGui::PushID(&p_Transform);
    if constexpr (D == D2)
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

static void editDirectionalLight(DirectionalLight &p_Light) noexcept
{
    ImGui::PushID(&p_Light);
    ImGui::SliderFloat("Intensity", &p_Light.DirectionAndIntensity.w, 0.f, 1.f);
    ImGui::SliderFloat3("Direction", glm::value_ptr(p_Light.DirectionAndIntensity), 0.f, 1.f);
    ImGui::ColorEdit3("Color", p_Light.Color.AsPointer());
    ImGui::PopID();
}

static void editPointLight(PointLight &p_Light) noexcept
{
    ImGui::PushID(&p_Light);
    ImGui::SliderFloat("Intensity", &p_Light.PositionAndIntensity.w, 0.f, 1.f);
    ImGui::DragFloat3("Position", glm::value_ptr(p_Light.PositionAndIntensity), 0.01f);
    ImGui::SliderFloat("Radius", &p_Light.Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::ColorEdit3("Color", p_Light.Color.AsPointer());
    ImGui::PopID();
}

void SWExampleLayer::renderLightSpawn() noexcept
{
    static int lightToSpawn = 0;
    static DirectionalLight dirLightToAdd{vec4{1.f, 1.f, 1.f, 1.f}, Color::WHITE};
    static PointLight pointLightToAdd{vec4{0.f, 0.f, 0.f, 1.f}, Color::WHITE, 1.f};
    static usize selectedDirLight = 0;
    static usize selectedPointLight = 0;

    ImGui::SliderFloat("Ambient intensity", &m_Ambient.w, 0.f, 1.f);
    ImGui::ColorEdit3("Color", glm::value_ptr(m_Ambient));

    ImGui::Combo("Light", &lightToSpawn, "Directional\0Point\0\0");
    if (lightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &m_DrawLights);

    if (ImGui::Button("Spawn##Light"))
    {
        if (lightToSpawn == 0)
            m_DirectionalLights.push_back(dirLightToAdd);
        else
            m_PointLights.push_back(pointLightToAdd);
    }

    const usize dsize = static_cast<usize>(m_DirectionalLights.size());
    for (usize i = 0; i < dsize; ++i)
    {
        ImGui::PushID(&m_DirectionalLights[i]);
        if (ImGui::Button("X"))
        {
            m_DirectionalLights.erase(m_DirectionalLights.begin() + i);
            ImGui::PopID();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Directional", selectedDirLight == i))
            selectedDirLight = i;
        ImGui::PopID();
    }
    if (selectedDirLight < dsize)
        editDirectionalLight(m_DirectionalLights[selectedDirLight]);

    const usize psize = static_cast<usize>(m_PointLights.size());
    for (usize i = 0; i < psize; ++i)
    {
        ImGui::PushID(&m_PointLights[i]);
        if (ImGui::Button("X"))
        {
            m_PointLights.erase(m_PointLights.begin() + i);
            ImGui::PopID();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Point", selectedPointLight == i))
            selectedPointLight = i;
        ImGui::PopID();
    }
    if (selectedPointLight < psize)
        editPointLight(m_PointLights[selectedPointLight]);
}

template <Dimension D> static void renderShapeSpawn(LayerData<D> &p_Data) noexcept
{
    static i32 ngonSides = 3;
    static f32 stadiumLength = 1.f;
    static f32 stadiumRadius = 0.5f;
    static f32 capsuleLength = 1.f;
    static f32 capsuleRadius = 0.5f;

    if constexpr (D == D2)
        ImGui::Combo("Shape", &p_Data.ShapeToSpawn, "Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0\0");
    else
        ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                     "Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0Cube\0Sphere\0Cylinder\0Capsule\0\0");

    if (p_Data.ShapeToSpawn == 3)
        ImGui::SliderInt("Sides", &ngonSides, 3, ONYX_MAX_REGULAR_POLYGON_SIDES);
    else if (p_Data.ShapeToSpawn == 4)
    {
        static vec<D> toAdd{0.f};
        if constexpr (D == D2)
            ImGui::DragFloat2("Vertex", glm::value_ptr(toAdd), 0.1f);
        else
            ImGui::DragFloat3("Vertex", glm::value_ptr(toAdd), 0.1f);
        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            p_Data.PolygonVertices.push_back(toAdd);
            toAdd = vec<D>{0.f};
        }
        for (usize i = 0; i < p_Data.PolygonVertices.size(); ++i)
        {
            ImGui::PushID(&p_Data.PolygonVertices[i]);
            if (ImGui::Button("X"))
            {
                p_Data.PolygonVertices.erase(p_Data.PolygonVertices.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::SameLine();
            if constexpr (D == D2)
                ImGui::Text("Vertex %zu: (%.2f, %.2f)", i, p_Data.PolygonVertices[i].x, p_Data.PolygonVertices[i].y);
            else
                ImGui::Text("Vertex %zu: (%.2f, %.2f, %.2f)", i, p_Data.PolygonVertices[i].x,
                            p_Data.PolygonVertices[i].y, p_Data.PolygonVertices[i].z);
            ImGui::PopID();
        }
    }
    else if (p_Data.ShapeToSpawn == 5)
    {
        ImGui::SliderFloat("Length", &stadiumLength, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Radius", &stadiumRadius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    }
    else if (p_Data.ShapeToSpawn == 9)
    {
        ImGui::SliderFloat("Length", &capsuleLength, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Radius", &capsuleRadius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    }

    if (ImGui::Button("Spawn##Shape"))
    {
        if (p_Data.ShapeToSpawn == 0)
            p_Data.Shapes.push_back(KIT::Scope<Triangle<D>>::Create());
        else if (p_Data.ShapeToSpawn == 1)
            p_Data.Shapes.push_back(KIT::Scope<Square<D>>::Create());
        else if (p_Data.ShapeToSpawn == 2)
            p_Data.Shapes.push_back(KIT::Scope<Circle<D>>::Create());
        else if (p_Data.ShapeToSpawn == 3)
        {
            auto ngon = KIT::Scope<NGon<D>>::Create();
            ngon->Sides = static_cast<u32>(ngonSides);
            p_Data.Shapes.push_back(std::move(ngon));
        }
        else if (p_Data.ShapeToSpawn == 4)
        {
            auto polygon = KIT::Scope<Polygon<D>>::Create();
            polygon->Vertices = p_Data.PolygonVertices;
            p_Data.Shapes.push_back(std::move(polygon));
            p_Data.PolygonVertices.clear();
        }
        else if (p_Data.ShapeToSpawn == 5)
        {
            auto stadium = KIT::Scope<Stadium<D>>::Create();
            stadium->Length = stadiumLength;
            stadium->Radius = stadiumRadius;
            p_Data.Shapes.push_back(std::move(stadium));
        }
        else if constexpr (D == D3)
        {
            if (p_Data.ShapeToSpawn == 6)
                p_Data.Shapes.push_back(KIT::Scope<Cube>::Create());
            else if (p_Data.ShapeToSpawn == 7)
                p_Data.Shapes.push_back(KIT::Scope<Sphere>::Create());
            else if (p_Data.ShapeToSpawn == 8)
                p_Data.Shapes.push_back(KIT::Scope<Cylinder>::Create());
            else if (p_Data.ShapeToSpawn == 9)
            {
                auto capsule = KIT::Scope<Capsule>::Create();
                capsule->Length = capsuleLength;
                capsule->Radius = capsuleRadius;
                p_Data.Shapes.push_back(std::move(capsule));
            }
        }
    }

    if (ImGui::Button("Clear"))
        p_Data.Shapes.clear();

    const usize size = static_cast<usize>(p_Data.Shapes.size());
    static usize selected = 0;
    for (usize i = 0; i < size; ++i)
    {
        ImGui::PushID(&p_Data.Shapes[i]);
        if (ImGui::Button("X"))
        {
            p_Data.Shapes.erase(p_Data.Shapes.begin() + i);
            ImGui::PopID();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable(p_Data.Shapes[i]->GetName(), selected == i))
            selected = i;
        ImGui::PopID();
    }
    if (selected < size)
    {
        ImGui::Text("Transform");
        editTransform(p_Data.Shapes[selected]->Transform);
        ImGui::Text("Material");
        editMaterial(p_Data.Shapes[selected]->Material);
        p_Data.Shapes[selected]->Edit();
    }
}

template <Dimension D>
static void processPolygonEvent(LayerData<D> &p_Data, const Event &p_Event, [[maybe_unused]] const f32 p_ZOffset)
{
    if (p_Event.Type != Event::MousePressed || p_Data.ShapeToSpawn != 4)
        return;
    if constexpr (D == D2)
        p_Data.PolygonVertices.push_back(p_Data.Context->GetMouseCoordinates());
    else
        p_Data.PolygonVertices.push_back(p_Data.Context->GetMouseCoordinates(p_ZOffset));
}

template <Dimension D>
static void processScrollEvent(LayerData<D> &p_Data, const Event &p_Event, Window *p_Window) noexcept
{
    if (p_Event.Type != Event::Scrolled || !Input::IsKeyPressed(p_Window, Input::Key::LeftShift))
        return;

    if (p_Data.ControlAsCamera)
        Transform<D>::ScaleExtrinsic(p_Data.AxesTransform, vec<D>{1.f + 0.005f * p_Event.ScrollOffset.y});
    else
        Transform<D>::ScaleIntrinsic(p_Data.AxesTransform, vec<D>{1.f + 0.005f * p_Event.ScrollOffset.y});
}

bool SWExampleLayer::OnEvent(const Event &p_Event) noexcept
{
    Window *window = m_Application->GetMainWindow();
    processPolygonEvent(m_LayerData2, p_Event, m_ZOffset);
    processPolygonEvent(m_LayerData3, p_Event, m_ZOffset);

    processScrollEvent(m_LayerData2, p_Event, window);
    processScrollEvent(m_LayerData3, p_Event, window);

    return true;
}

template <Dimension D> void SWExampleLayer::controlAxes(LayerData<D> &p_Data) noexcept
{
    Transform<D> t{};
    const f32 step = m_Application->GetDeltaTime().AsSeconds();

    Window *window = m_Application->GetMainWindow();
    if (Input::IsKeyPressed(window, Input::Key::A))
        t.Translation.x -= step;
    if (Input::IsKeyPressed(window, Input::Key::D))
        t.Translation.x += step;

    if constexpr (D == D2)
    {
        if (Input::IsKeyPressed(window, Input::Key::W))
            t.Translation.y += step;
        if (Input::IsKeyPressed(window, Input::Key::S))
            t.Translation.y -= step;

        if (Input::IsKeyPressed(window, Input::Key::Q))
            t.Rotation += step;
        if (Input::IsKeyPressed(window, Input::Key::E))
            t.Rotation -= step;
    }
    else
    {
        if (Input::IsKeyPressed(window, Input::Key::W))
            t.Translation.z -= step;
        if (Input::IsKeyPressed(window, Input::Key::S))
            t.Translation.z += step;
        static vec2 prevMpos = Input::GetMousePosition(window);
        const vec2 mpos = Input::GetMousePosition(window);
        const vec2 delta = Input::IsKeyPressed(window, Input::Key::LeftShift) ? 3.f * (mpos - prevMpos) : vec2{0.f};

        prevMpos = mpos;

        vec3 angles{delta.y, delta.x, 0.f};
        if (Input::IsKeyPressed(window, Input::Key::Q))
            angles.z -= step;
        if (Input::IsKeyPressed(window, Input::Key::E))
            angles.z += step;

        if (angles.x > 0.f)
            ImGui::Text("X Rotation: +");
        else if (angles.x < 0.f)
            ImGui::Text("X Rotation: -");
        else
            ImGui::Text("X Rotation: ");
        if (angles.y > 0.f)
            ImGui::Text("Y Rotation: +");
        else if (angles.y < 0.f)
            ImGui::Text("Y Rotation: -");
        else
            ImGui::Text("Y Rotation: ");
        if (angles.z > 0.f)
            ImGui::Text("Z Rotation: +");
        else if (angles.z < 0.f)
            ImGui::Text("Z Rotation: -");
        else
            ImGui::Text("Z Rotation: ");

        t.Rotation = quat{angles};
    }

    if (p_Data.ControlAsCamera)
    {
        t.Translation = -t.Translation;
        p_Data.AxesTransform = t.ComputeTransform() * p_Data.AxesTransform;
    }
    else
        p_Data.AxesTransform *= t.ComputeTransform();
}

template <Dimension D> void SWExampleLayer::renderUI(LayerData<D> &p_Data) noexcept
{
    if (ImGui::Begin(D == D2 ? "2D" : "3D"))
    {
        if constexpr (D == D2)
        {
            const vec2 mpos2 = m_LayerData2.Context->GetMouseCoordinates();
            ImGui::Text("Mouse Position: (%.2f, %.2f)", mpos2.x, mpos2.y);
        }
        else
        {
            ImGui::SliderFloat("Mouse Z offset", &m_ZOffset, -1.f, 1.f);
            const vec3 mpos3 = m_LayerData3.Context->GetMouseCoordinates(m_ZOffset);
            ImGui::Text("Mouse Position 3D: (%.2f, %.2f, %.2f)", mpos3.x, mpos3.y, mpos3.z);
        }

        if (ImGui::CollapsingHeader("Axes"))
        {
            static bool control = false;
            ImGui::Checkbox("Control", &control);
            if (control)
            {
                ImGui::Checkbox("Control as camera", &p_Data.ControlAsCamera);
                controlAxes<D>(p_Data);
            }

            if constexpr (D == D3)
            {
                ImGui::Checkbox("Perspective", &m_Perspective);
                if (m_Perspective)
                {
                    static f32 rads = 75.f;
                    if (ImGui::SliderFloat("Field of view", &rads, 75.f, 90.f))
                        m_FieldOfView = glm::radians(rads);
                    ImGui::SliderFloat("Near", &m_Near, 0.1f, 10.f);
                    ImGui::SliderFloat("Far", &m_Far, 10.f, 100.f);
                }
            }

            ImGui::Checkbox("Draw##Axes", &p_Data.DrawAxes);
            if (p_Data.DrawAxes)
                ImGui::SliderFloat("Axes thickness", &p_Data.AxesThickness, 0.001f, 0.1f);

            if (ImGui::TreeNode("Material"))
            {
                editMaterial(p_Data.AxesMaterial);
                ImGui::TreePop();
            }
        }
        if (ImGui::CollapsingHeader("Shapes"))
            renderShapeSpawn(p_Data);
        if constexpr (D == D3)
            if (ImGui::CollapsingHeader("Lights"))
                renderLightSpawn();
    }
    ImGui::End();
}

void SWExampleLayer::OnRender() noexcept
{
    drawShapes(m_LayerData2);
    drawShapes(m_LayerData3);

    ImGui::ShowDemoWindow();
    ImPlot::ShowDemoWindow();

    if (ImGui::Begin("Global settings"))
    {
        static f32 maxTS = 0.f;
        static f32 smoothTS = 0.f;
        static f32 smoothFactor = 0.f;
        const f32 ts = m_Application->GetDeltaTime().AsMilliseconds();
        if (ts > maxTS)
            maxTS = ts;
        smoothTS = smoothFactor * smoothFactor + (1.f - smoothFactor) * ts;

        ImGui::Text("Time step: %.2f ms (max: %.2f ms)", smoothTS, maxTS);
        ImGui::SliderFloat("Smoothing factor", &smoothFactor, 0.f, 0.999f);
        if (ImGui::Button("Reset maximum"))
            maxTS = 0.f;
        ImGui::ColorEdit3("Background", m_BackgroundColor.AsPointer());
    }
    ImGui::End();

    renderUI(m_LayerData2);
    renderUI(m_LayerData3);
}

} // namespace ONYX