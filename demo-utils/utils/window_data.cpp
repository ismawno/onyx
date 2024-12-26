#include "utils/window_data.hpp"
#include "onyx/core/shaders.hpp"
#include <imgui.h>
#include <implot.h>

namespace Onyx
{
static const VKit::Shader &getBlurShader() noexcept
{
    static VKit::Shader shader{};
    if (!shader)
    {
        shader = CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/blur.frag");
        Core::GetDeletionQueue().SubmitForDeletion(shader);
    }
    return shader;
}

static const VKit::PipelineLayout &getBlurPipelineLayout(const PostProcessing *p_PostProcessing) noexcept
{
    static VKit::PipelineLayout layout{};
    if (!layout)
    {
        VKit::PipelineLayout::Builder builder = p_PostProcessing->CreatePipelineLayoutBuilder();
        const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
        VKIT_ASSERT_RESULT(result);
        layout = result.GetValue();

        Core::GetDeletionQueue().SubmitForDeletion(layout);
    }
    return layout;
}

static const VKit::Shader &getRainbowShader() noexcept
{
    static VKit::Shader shader{};
    if (!shader)
    {
        shader = CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");
        Core::GetDeletionQueue().SubmitForDeletion(shader);
    }
    return shader;
}

static const VKit::PipelineLayout &getRainbowPipelineLayout() noexcept
{
    static VKit::PipelineLayout layout{};
    if (!layout)
    {
        const auto result = VKit::PipelineLayout::Builder(Core::GetDevice()).Build();
        VKIT_ASSERT_RESULT(result);
        layout = result.GetValue();

        Core::GetDeletionQueue().SubmitForDeletion(layout);
    }
    return layout;
}

void WindowData::OnStart(Window *p_Window) noexcept
{
    m_LayerData2.Context = p_Window->GetRenderContext<D2>();
    m_LayerData3.Context = p_Window->GetRenderContext<D3>();
    m_Window = p_Window;

    const auto result =
        VKit::GraphicsPipeline::Builder(Core::GetDevice(), getRainbowPipelineLayout(), m_Window->GetRenderPass())
            .SetViewportCount(1)
            .AddShaderStage(GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(getRainbowShader(), VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .AddDefaultColorAttachment()
            .Build();

    VKIT_ASSERT_RESULT(result);
    const VKit::GraphicsPipeline &pipeline = result.GetValue();
    Core::GetDeletionQueue().SubmitForDeletion(pipeline);

    m_RainbowJob = VKit::GraphicsJob(result.GetValue(), getRainbowPipelineLayout());
}

void WindowData::OnUpdate() noexcept
{
    if (!m_PostProcessing)
        return;
    m_BlurData.Width = static_cast<f32>(m_Window->GetPixelWidth());
    m_BlurData.Height = static_cast<f32>(m_Window->GetPixelHeight());
    m_Window->GetPostProcessing()->UpdatePushConstantRange(0, &m_BlurData);
}

void WindowData::OnRender(const VkCommandBuffer p_CommandBuffer, const TKit::Timespan p_Timestep) noexcept
{
    std::scoped_lock lock(*m_Mutex);
    drawShapes(m_LayerData2, p_Timestep);
    drawShapes(m_LayerData3, p_Timestep);

    if (m_RainbowBackground)
    {
        m_RainbowJob.Bind(p_CommandBuffer);
        m_RainbowJob.Draw(p_CommandBuffer, 3);
    }
}

void WindowData::OnImGuiRender() noexcept
{
    ImGui::ColorEdit3("Window background", m_BackgroundColor.AsPointer());
    const VkPresentModeKHR currentMode = m_Window->GetPresentMode();
    bool vsync = currentMode == VK_PRESENT_MODE_FIFO_KHR;
    if (ImGui::Checkbox("VSync", &vsync))
        m_Window->SetPresentMode(vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR);

    ImGui::Checkbox("Rainbow background", &m_RainbowBackground);
    if (ImGui::TreeNode("Post-processing"))
    {
        if (ImGui::Checkbox("Enable", &m_PostProcessing))
        {
            if (m_PostProcessing)
            {
                PostProcessing *postProcessing = m_Window->GetPostProcessing();
                m_Window->SetupPostProcessing(getBlurPipelineLayout(postProcessing), getBlurShader());
            }
            else
                m_Window->RemovePostProcessing();
        }
        ImGui::SliderInt("Blur kernel size", (int *)&m_BlurData.KernelSize, 0, 12);
        ImGui::TreePop();
    }

    ImGui::BeginTabBar("Dimension");
    if (ImGui::BeginTabItem("2D"))
    {
        renderUI(m_LayerData2);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("3D"))
    {
        renderUI(m_LayerData3);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

template <Dimension D> static void processPolygonEvent(LayerData<D> &p_Data, const Event &p_Event)
{
    if (p_Event.Type != Event::MousePressed || p_Data.ShapeToSpawn != 4)
        return;
    if constexpr (D == D2)
        p_Data.PolygonVertices.push_back(p_Data.Context->GetMouseCoordinates());
    else
        p_Data.PolygonVertices.push_back(p_Data.Context->GetMouseCoordinates(p_Data.ZOffset));
}

void WindowData::OnEvent(const Event &p_Event) noexcept
{
    processPolygonEvent(m_LayerData2, p_Event);
    processPolygonEvent(m_LayerData3, p_Event);
    if (p_Event.Type == Event::Scrolled && Input::IsKeyPressed(p_Event.Window, Input::Key::LeftShift))
        m_LayerData2.Context->ApplyCameraLikeScalingControls(0.005f * p_Event.ScrollOffset.y);
}

void WindowData::OnImGuiRenderGlobal(const TKit::Timespan p_Timestep) noexcept
{
    ImGui::ShowDemoWindow();
    ImPlot::ShowDemoWindow();

    if (ImGui::Begin("Global stats"))
    {
        static f32 maxTS = 0.f;
        static f32 smoothTS = 0.f;
        static f32 smoothFactor = 0.f;
        const f32 ts = p_Timestep.AsMilliseconds();
        if (ts > maxTS)
            maxTS = ts;
        smoothTS = smoothFactor * smoothTS + (1.f - smoothFactor) * ts;

        ImGui::Text("Time step: %.2f ms (max: %.2f ms)", smoothTS, maxTS);
        ImGui::SliderFloat("Smoothing factor", &smoothFactor, 0.f, 0.999f);
        if (ImGui::Button("Reset maximum"))
            maxTS = 0.f;
    }
    ImGui::End();
}

template <Dimension D> void WindowData::drawShapes(const LayerData<D> &p_Data, const TKit::Timespan p_Timestep) noexcept
{
    p_Data.Context->Flush(m_BackgroundColor);

    const f32 step = p_Timestep.AsSeconds();
    p_Data.Context->ApplyCameraLikeMovementControls(step, step);

    p_Data.Context->TransformAxes(p_Data.AxesTransform.ComputeTransform());

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
        p_Data.Context->AmbientColor(p_Data.Ambient);
        for (const auto &light : p_Data.DirectionalLights)
        {
            p_Data.Context->LightColor(light.Color);
            p_Data.Context->DirectionalLight(light);
        }
        for (const auto &light : p_Data.PointLights)
        {
            if (p_Data.DrawLights)
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

template <Dimension D> static void renderShapeSpawn(LayerData<D> &p_Data) noexcept
{
    static i32 ngonSides = 3;

    if constexpr (D == D2)
        ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                     "Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0Rounded Square\0\0");
    else
        ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                     "Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0Rounded "
                     "Square\0Cube\0Sphere\0Cylinder\0Capsule\0Rounded Cube\0\0");

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

    if (ImGui::Button("Spawn##Shape"))
    {
        if (p_Data.ShapeToSpawn == 0)
            p_Data.Shapes.push_back(TKit::Scope<Triangle<D>>::Create());
        else if (p_Data.ShapeToSpawn == 1)
            p_Data.Shapes.push_back(TKit::Scope<Square<D>>::Create());
        else if (p_Data.ShapeToSpawn == 2)
            p_Data.Shapes.push_back(TKit::Scope<Circle<D>>::Create());
        else if (p_Data.ShapeToSpawn == 3)
        {
            auto ngon = TKit::Scope<NGon<D>>::Create();
            ngon->Sides = static_cast<u32>(ngonSides);
            p_Data.Shapes.push_back(std::move(ngon));
        }
        else if (p_Data.ShapeToSpawn == 4)
        {
            auto polygon = TKit::Scope<Polygon<D>>::Create();
            polygon->Vertices = p_Data.PolygonVertices;
            p_Data.Shapes.push_back(std::move(polygon));
            p_Data.PolygonVertices.clear();
        }
        else if (p_Data.ShapeToSpawn == 5)
            p_Data.Shapes.push_back(TKit::Scope<Stadium<D>>::Create());
        else if (p_Data.ShapeToSpawn == 6)
            p_Data.Shapes.push_back(TKit::Scope<RoundedSquare<D>>::Create());

        else if constexpr (D == D3)
        {
            if (p_Data.ShapeToSpawn == 7)
                p_Data.Shapes.push_back(TKit::Scope<Cube>::Create());
            else if (p_Data.ShapeToSpawn == 8)
                p_Data.Shapes.push_back(TKit::Scope<Sphere>::Create());
            else if (p_Data.ShapeToSpawn == 9)
                p_Data.Shapes.push_back(TKit::Scope<Cylinder>::Create());
            else if (p_Data.ShapeToSpawn == 10)
                p_Data.Shapes.push_back(TKit::Scope<Capsule>::Create());
            else if (p_Data.ShapeToSpawn == 11)
                p_Data.Shapes.push_back(TKit::Scope<RoundedCube>::Create());
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

template <Dimension D> void WindowData::renderUI(LayerData<D> &p_Data) noexcept
{
    if constexpr (D == D2)
    {
        std::scoped_lock lock(*m_Mutex);
        const vec2 mpos2 = m_LayerData2.Context->GetMouseCoordinates();
        ImGui::Text("Mouse Position: (%.2f, %.2f)", mpos2.x, mpos2.y);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &p_Data.ZOffset, -1.f, 1.f);

        std::scoped_lock lock(*m_Mutex);
        const vec3 mpos3 = m_LayerData3.Context->GetMouseCoordinates(p_Data.ZOffset);

        ImGui::Text("Mouse Position 3D: (%.2f, %.2f, %.2f)", mpos3.x, mpos3.y, mpos3.z);
    }

    if (ImGui::CollapsingHeader("Axes"))
    {
        ImGui::Text("Transform");
        editTransform<D>(p_Data.AxesTransform);
        if constexpr (D == D3)
        {
            if (ImGui::Checkbox("Perspective", &p_Data.Perspective))
            {
                if (p_Data.Perspective)
                    p_Data.Context->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
                else
                    p_Data.Context->SetOrthographicProjection();
            }

            if (p_Data.Perspective)
            {
                bool changed = false;
                f32 degs = glm::degrees(p_Data.FieldOfView);

                changed |= ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f);
                changed |= ImGui::SliderFloat("Near", &p_Data.Near, 0.1f, 10.f);
                changed |= ImGui::SliderFloat("Far", &p_Data.Far, 10.f, 100.f);
                if (changed)
                {
                    p_Data.FieldOfView = glm::radians(degs);
                    p_Data.Context->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
                }
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

void WindowData::renderLightSpawn() noexcept
{
    ImGui::SliderFloat("Ambient intensity", &m_LayerData3.Ambient.w, 0.f, 1.f);
    ImGui::ColorEdit3("Color", glm::value_ptr(m_LayerData3.Ambient));

    ImGui::Combo("Light", &m_LayerData3.LightToSpawn, "Directional\0Point\0\0");
    if (m_LayerData3.LightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &m_LayerData3.DrawLights);

    if (ImGui::Button("Spawn##Light"))
    {
        if (m_LayerData3.LightToSpawn == 0)
            m_LayerData3.DirectionalLights.push_back(m_LayerData3.DirLightToAdd);
        else
            m_LayerData3.PointLights.push_back(m_LayerData3.PointLightToAdd);
    }

    const usize dsize = static_cast<usize>(m_LayerData3.DirectionalLights.size());
    for (usize i = 0; i < dsize; ++i)
    {
        ImGui::PushID(&m_LayerData3.DirectionalLights[i]);
        if (ImGui::Button("X"))
        {
            m_LayerData3.DirectionalLights.erase(m_LayerData3.DirectionalLights.begin() + i);
            ImGui::PopID();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Directional", m_LayerData3.SelectedDirLight == i))
            m_LayerData3.SelectedDirLight = i;
        ImGui::PopID();
    }
    if (m_LayerData3.SelectedDirLight < dsize)
        editDirectionalLight(m_LayerData3.DirectionalLights[m_LayerData3.SelectedDirLight]);

    const usize psize = static_cast<usize>(m_LayerData3.PointLights.size());
    for (usize i = 0; i < psize; ++i)
    {
        ImGui::PushID(&m_LayerData3.PointLights[i]);
        if (ImGui::Button("X"))
        {
            m_LayerData3.PointLights.erase(m_LayerData3.PointLights.begin() + i);
            ImGui::PopID();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Selectable("Point", m_LayerData3.SelectedPointLight == i))
            m_LayerData3.SelectedPointLight = i;
        ImGui::PopID();
    }
    if (m_LayerData3.SelectedPointLight < psize)
        editPointLight(m_LayerData3.PointLights[m_LayerData3.SelectedPointLight]);
}

} // namespace Onyx