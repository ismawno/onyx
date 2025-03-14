#include "utils/window_data.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/app/user_layer.hpp"
#include <imgui.h>
#include <implot.h>

// dirty ass macros as lazy enums lol
#define TRIANGLE 0
#define SQUARE 1
#define CIRCLE 2
#define NGON 3
#define POLYGON 4
#define STADIUM 5
#define ROUNDED_SQUARE 6
#define CUBE 7
#define SPHERE 8
#define CYLINDER 9
#define CAPSULE 10
#define ROUNDED_CUBE 11

namespace Onyx::Demo
{
static const VKit::PipelineLayout &getRainbowLayout() noexcept
{
    static VKit::PipelineLayout layout{};
    if (layout)
        return layout;
    const auto result = VKit::PipelineLayout::Builder(Core::GetDevice()).Build();
    VKIT_ASSERT_RESULT(result);
    layout = result.GetValue();
    Core::GetDeletionQueue().SubmitForDeletion(layout);
    return layout;
}

static const VKit::Shader &getRainbowShader() noexcept
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}

static const VKit::Shader &getBlurShader() noexcept
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/blur.frag");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}

void WindowData::OnStart(Window *p_Window) noexcept
{
    m_LayerData2.Context = p_Window->GetRenderContext<D2>();
    m_LayerData3.Context = p_Window->GetRenderContext<D3>();
    m_Window = p_Window;

    const auto presult =
        VKit::GraphicsPipeline::Builder(Core::GetDevice(), getRainbowLayout(), m_Window->GetRenderPass())
            .SetViewportCount(1)
            .AddShaderStage(GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(getRainbowShader(), VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .AddDefaultColorAttachment()
            .Build();

    VKIT_ASSERT_RESULT(presult);
    const VKit::GraphicsPipeline &pipeline = presult.GetValue();

    m_RainbowJob = VKit::GraphicsJob(presult.GetValue(), getRainbowLayout());

    VKit::PipelineLayout::Builder builder = m_Window->GetPostProcessing()->CreatePipelineLayoutBuilder();
    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    m_BlurLayout = result.GetValue();

    Core::GetDeletionQueue().SubmitForDeletion(pipeline);
    Core::GetDeletionQueue().SubmitForDeletion(m_BlurLayout);
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
    UserLayer::PresentModeEditor(m_Window, UserLayer::Flag_DisplayHelp);

    ImGui::Checkbox("Rainbow background", &m_RainbowBackground);
    UserLayer::HelpMarkerSameLine("This is a small demonstration of how to hook-up your own pipelines to the Onyx "
                                  "rendering context (in this case, to draw a nice rainbow background).");

    if (ImGui::Checkbox("Blur", &m_PostProcessing))
    {
        if (m_PostProcessing)
        {
            m_BlurData.Width = static_cast<f32>(m_Window->GetPixelWidth());
            m_BlurData.Height = static_cast<f32>(m_Window->GetPixelHeight());
            m_Window->SetPostProcessing(m_BlurLayout, getBlurShader())->UpdatePushConstantRange(0, &m_BlurData);
        }
        else
            m_Window->RemovePostProcessing();
    }
    UserLayer::HelpMarkerSameLine(
        "This is a small demonstration of how to hook-up a post-processing pipeline to the Onyx rendering context to "
        "apply transformations to the final image (in this case, a blur effect).");

    if (m_PostProcessing)
        ImGui::SliderInt("Blur kernel size", (int *)&m_BlurData.KernelSize, 0, 12);

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
    if (p_Event.Type != Event::MousePressed || p_Data.ShapeToSpawn != POLYGON)
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
        m_LayerData2.Context->ApplyCameraScalingControls(0.005f * p_Event.ScrollOffset.y);
}

void WindowData::OnImGuiRenderGlobal(const TKit::Timespan p_Timestep) noexcept
{
    ImGui::ShowDemoWindow();
    ImPlot::ShowDemoWindow();

    if (ImGui::Begin("Welcome to Onyx, my Vulkan application framework!"))
    {
        UserLayer::DisplayFrameTime(p_Timestep, UserLayer::Flag_DisplayHelp);
        ImGui::TextWrapped(
            "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
            "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
            "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
            "The framework is still in its early stages, but I plan to expand it further in the future.");

        ImGui::TextWrapped("This program is the Onyx demo, showcasing some of its features. Most of them can be tried "
                           "in the 'Editor' panel.");

        ImGui::TextLinkOpenURL("My GitHub", "https://github.com/ismawno");
    }
    ImGui::End();
}
void WindowData::RenderEditorText() noexcept
{
    ImGui::Text("This is the editor panel, where you can interact with the demo.");
    ImGui::TextWrapped("Onyx windows can draw shapes in 2D and 3D, and have a separate API for each even though the "
                       "window is shared. You can choose between both dimensions using the tabs below.");
}

template <Dimension D> void WindowData::drawShapes(const LayerData<D> &p_Data, const TKit::Timespan p_Timestep) noexcept
{
    p_Data.Context->Flush(m_BackgroundColor);
    p_Data.Context->ApplyCameraMovementControls(p_Timestep);
    p_Data.Context->TransformAxes(p_Data.AxesTransform.ComputeTransform());

    const LatticeData<D> &lattice = p_Data.Lattice;
    if (lattice.Enabled && lattice.Shape && p_Data.ShapeToSpawn != POLYGON)
    {
        const fvec<D> separation =
            lattice.PropToScale ? lattice.Shape->Transform.Scale * lattice.Separation : fvec<D>{lattice.Separation};
        const fvec<D> midPoint = 0.5f * separation * fvec<D>{lattice.Dimensions - 1u};

        for (u32 i = 0; i < lattice.Dimensions.x; ++i)
        {
            const f32 x = static_cast<f32>(i) * separation.x;
            for (u32 j = 0; j < lattice.Dimensions.y; ++j)
            {
                const f32 y = static_cast<f32>(j) * separation.y;
                if constexpr (D == D2)
                {
                    lattice.Shape->Transform.Translation = fvec2{x, y} - midPoint;
                    lattice.Shape->Draw(p_Data.Context);
                }
                else
                    for (u32 k = 0; k < lattice.Dimensions.z; ++k)
                    {
                        const f32 z = static_cast<f32>(k) * separation.z;
                        lattice.Shape->Transform.Translation = fvec3{x, y, z} - midPoint;
                        lattice.Shape->Draw(p_Data.Context);
                    }
            }
        }
    }

    for (const auto &shape : p_Data.Shapes)
        shape->Draw(p_Data.Context);

    p_Data.Context->Outline(false);
    if (p_Data.DrawAxes)
    {
        p_Data.Context->Material(p_Data.AxesMaterial);
        p_Data.Context->Fill();
        p_Data.Context->Axes({.Thickness = p_Data.AxesThickness});
    }

    for (const fvec2 &vertex : p_Data.PolygonVertices)
    {
        p_Data.Context->Push();
        p_Data.Context->Scale(0.02f);
        if constexpr (D == D2)
            p_Data.Context->Translate(vertex);
        else
            p_Data.Context->Translate(fvec3{vertex, 0.f});
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

template <Dimension D> static void renderShapeSpawn(LayerData<D> &p_Data) noexcept
{
    const auto createShape = [&p_Data]() -> TKit::Scope<Shape<D>> {
        if (p_Data.ShapeToSpawn == TRIANGLE)
            return TKit::Scope<Triangle<D>>::Create();
        else if (p_Data.ShapeToSpawn == SQUARE)
            return TKit::Scope<Square<D>>::Create();
        else if (p_Data.ShapeToSpawn == CIRCLE)
            return TKit::Scope<Circle<D>>::Create();
        else if (p_Data.ShapeToSpawn == NGON)
        {
            auto ngon = TKit::Scope<NGon<D>>::Create();
            ngon->Sides = static_cast<u32>(p_Data.NGonSides);
            return std::move(ngon);
        }
        else if (p_Data.ShapeToSpawn == POLYGON)
        {
            auto polygon = TKit::Scope<Polygon<D>>::Create();
            polygon->Vertices = p_Data.PolygonVertices;
            p_Data.PolygonVertices.clear();
            return std::move(polygon);
        }
        else if (p_Data.ShapeToSpawn == STADIUM)
            return TKit::Scope<Stadium<D>>::Create();
        else if (p_Data.ShapeToSpawn == ROUNDED_SQUARE)
            return TKit::Scope<RoundedSquare<D>>::Create();

        else if constexpr (D == D3)
        {
            if (p_Data.ShapeToSpawn == CUBE)
                return TKit::Scope<Cube>::Create();
            else if (p_Data.ShapeToSpawn == SPHERE)
                return TKit::Scope<Sphere>::Create();
            else if (p_Data.ShapeToSpawn == CYLINDER)
                return TKit::Scope<Cylinder>::Create();
            else if (p_Data.ShapeToSpawn == CAPSULE)
                return TKit::Scope<Capsule>::Create();
            else if (p_Data.ShapeToSpawn == ROUNDED_CUBE)
                return TKit::Scope<RoundedCube>::Create();
        }
        return nullptr;
    };

    const bool canSpawn = p_Data.ShapeToSpawn != POLYGON || p_Data.PolygonVertices.size() >= 3;
    if (!canSpawn)
        ImGui::TextDisabled("A polygon must have at least 3 verticesto spawn!");
    else if (ImGui::Button("Spawn##Shape"))
        p_Data.Shapes.push_back(createShape());

    if (canSpawn)
        ImGui::SameLine();

    bool changed = false;
    if constexpr (D == D2)
        changed |= ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                                "Triangle\0Square\0Circle\0NGon\0Convex Polygon\0Stadium\0Rounded Square\0\0");
    else
        changed |= ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                                "Triangle\0Square\0Circle\0NGon\0Convex Polygon\0Stadium\0Rounded "
                                "Square\0Cube\0Sphere\0Cylinder\0Capsule\0Rounded Cube\0\0");

    LatticeData<D> &lattice = p_Data.Lattice;
    if (!lattice.Shape)
        lattice.Shape = createShape();

    if (changed)
    {
        const Transform<D> transform = lattice.Shape->Transform;
        lattice.Shape = createShape();
        lattice.Shape->Transform = transform;
    }

    if (p_Data.ShapeToSpawn == NGON)
        ImGui::SliderInt("Sides", &p_Data.NGonSides, 3, ONYX_MAX_REGULAR_POLYGON_SIDES);
    else if (p_Data.ShapeToSpawn == POLYGON)
    {
        ImGui::Text("Vertices must be in counter clockwise order for outlines to work correctly");
        ImGui::Text("Click on the screen or the 'Add' button to add vertices to the polygon.");
        ImGui::DragFloat2("Vertex", glm::value_ptr(p_Data.VertexToAdd), 0.1f);

        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            p_Data.PolygonVertices.push_back(p_Data.VertexToAdd);
            p_Data.VertexToAdd = fvec2{0.f};
        }
        for (u32 i = 0; i < p_Data.PolygonVertices.size(); ++i)
        {
            ImGui::PushID(&p_Data.PolygonVertices[i]);
            if (ImGui::Button("X"))
            {
                p_Data.PolygonVertices.erase(p_Data.PolygonVertices.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::SameLine();
            ImGui::Text("Vertex %u: (%.2f, %.2f)", i, p_Data.PolygonVertices[i].x, p_Data.PolygonVertices[i].y);
            ImGui::PopID();
        }
    }

    if (ImGui::TreeNode("Lattice"))
    {
        ImGui::Checkbox("Draw shape lattice", &lattice.Enabled);
        UserLayer::HelpMarkerSameLine("You may choose to draw a lattice of shapes to stress test the rendering engine. "
                                      "I advice to build the engine "
                                      "in distribution mode to see meaningful results.");

        if (p_Data.ShapeToSpawn == POLYGON)
            ImGui::TextDisabled("Polygon shapes cannot be drawn in a lattice :(");
        if constexpr (D == D2)
        {
            ImGui::Text("Shape count: %u", lattice.Dimensions.x * lattice.Dimensions.y);
            ImGui::DragInt2("Lattice dimensions", reinterpret_cast<i32 *>(glm::value_ptr(lattice.Dimensions)), 2.f, 1,
                            INT32_MAX);
        }
        else
        {
            ImGui::Text("Shape count: %u", lattice.Dimensions.x * lattice.Dimensions.y * lattice.Dimensions.z);
            ImGui::DragInt3("Lattice dimensions", reinterpret_cast<i32 *>(glm::value_ptr(lattice.Dimensions)), 2.f, 1,
                            INT32_MAX);
        }

        ImGui::Checkbox("Separation proportional to scale", &lattice.PropToScale);
        ImGui::DragFloat("Lattice separation", &lattice.Separation, 0.01f, 0.f, FLT_MAX);
        if (lattice.Shape)
        {
            ImGui::Text("Lattice shape:");
            lattice.Shape->Edit();
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Line test"))
    {
        LineTest<D> &line = p_Data.Line;

        ImGui::Checkbox("Rounded", &line.Rounded);
        ImGui::Checkbox("Outline", &line.Outline);
        ImGui::SliderFloat("Outline width", &line.OutlineWidth, 0.01f, 0.1f);
        ImGui::SliderFloat("Thickness", &line.Thickness, 0.01f, 0.1f);

        if constexpr (D == D2)
        {
            ImGui::DragFloat2("Start", glm::value_ptr(line.Start), 0.1f);
            ImGui::DragFloat2("End", glm::value_ptr(line.End), 0.1f);
        }
        else
        {
            ImGui::DragFloat3("Start", glm::value_ptr(line.Start), 0.1f);
            ImGui::DragFloat3("End", glm::value_ptr(line.End), 0.1f);
        }

        ImGui::Text("Material");
        UserLayer::MaterialEditor<D>(line.Material, UserLayer::Flag_DisplayHelp);
        ImGui::ColorEdit3("Outline color", line.OutlineColor.AsPointer());

        p_Data.Context->Push();
        if (line.Outline)
        {
            p_Data.Context->Outline(line.OutlineColor);
            p_Data.Context->OutlineWidth(line.OutlineWidth);
        }
        p_Data.Context->Material(line.Material);
        if (line.Rounded)
            p_Data.Context->RoundedLine(line.Start, line.End, line.Thickness);
        else
            p_Data.Context->Line(line.Start, line.End, line.Thickness);
        p_Data.Context->Pop();
        ImGui::TreePop();
    }

    if (!p_Data.Shapes.empty() && ImGui::TreeNode("Spawned shapes"))
    {
        if (ImGui::Button("Clear"))
            p_Data.Shapes.clear();

        for (u32 i = 0; i < p_Data.Shapes.size(); ++i)
        {
            ImGui::PushID(&p_Data.Shapes[i]);
            if (ImGui::Button("X"))
            {
                p_Data.Shapes.erase(p_Data.Shapes.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            if (ImGui::Selectable(p_Data.Shapes[i]->GetName(), p_Data.SelectedShape == i))
                p_Data.SelectedShape = i;
            ImGui::PopID();
        }
        if (p_Data.SelectedShape < p_Data.Shapes.size())
            p_Data.Shapes[p_Data.SelectedShape]->Edit();
        ImGui::TreePop();
    }
}

template <Dimension D> void WindowData::renderUI(LayerData<D> &p_Data) noexcept
{
    const fvec2 spos = Input::GetMousePosition(m_Window);
    ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos.x, spos.y);
    UserLayer::HelpMarkerSameLine(
        "The screen mouse position is always normalized to the window size, always ranging "
        "from (-1, -1) to (1, 1) for 'x' and 'y', and from (0, 0) to (1, 1) for 'z' respectively.");

    if constexpr (D == D2)
    {
        const fvec2 wpos2 = m_LayerData2.Context->GetMouseCoordinates();
        ImGui::Text("World mouse position 2D: (%.2f, %.2f)", wpos2.x, wpos2.y);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &p_Data.ZOffset, 0.f, 1.f);
        UserLayer::HelpMarkerSameLine(
            "In 3D, the world mouse position can be ambiguous because of the extra dimension. This amibiguity needs to "
            "somehow be resolved. In most use-cases, ray casting is the best approach to fully define this position, "
            "but because this is a simple demo, the z offset can be manually specified, and is in the range [0, 1] "
            "(screen coordinates). Note that, if in perspective mode, 0 corresponds to the near plane and 1 to the "
            "far plane.");

        const fvec3 mpos3 = m_LayerData3.Context->GetMouseCoordinates(p_Data.ZOffset);
        ImGui::Text("Mouse position 3D: (%.2f, %.2f, %.2f)", mpos3.x, mpos3.y, mpos3.z);
    }
    UserLayer::HelpMarkerSameLine("The world mouse position has world units, meaning it is scaled to the world "
                                  "coordinates and are compatible with the translation units of the shapes.");

    if (ImGui::CollapsingHeader("Shapes"))
        renderShapeSpawn(p_Data);
    if constexpr (D == D3)
        if (ImGui::CollapsingHeader("Lights"))
            renderLightSpawn();

    if (ImGui::CollapsingHeader("Axes"))
    {
        ImGui::TextWrapped(
            "The axes are the coordinate system that is used to draw objects in the scene. All object "
            "positions will always be relative to the state the axes were in the moment the draw command was issued.");
        ImGui::Text("Transform");
        ImGui::SameLine();
        UserLayer::TransformEditor<D>(p_Data.AxesTransform, UserLayer::Flag_DisplayHelp);

        ImGui::Checkbox("Draw##Axes", &p_Data.DrawAxes);
        if (p_Data.DrawAxes)
            ImGui::SliderFloat("Axes thickness", &p_Data.AxesThickness, 0.001f, 0.1f);

        if (ImGui::TreeNode("Material"))
        {
            ImGui::SameLine();
            UserLayer::MaterialEditor<D>(p_Data.AxesMaterial, UserLayer::Flag_DisplayHelp);
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Projection & View"))
    {
        const Transform<D> &view = p_Data.Context->GetViewTransformInCurrentAxes();
        ImGui::Text("View transform (with respect current axes)");
        UserLayer::HelpMarkerSameLine(
            "This view transform is represented specifically with respect the current axes, "
            "but note that, as the view is a global state that is not reset every frame in "
            "the Onyx render context, it is generally detached from the axes transform. Onyx, behind the scenes, uses "
            "the detached view transform to setup the scene. This not a design decision but a requirement, as the axes "
            "is a somewhat volatile state (it is reset every frame).");

        UserLayer::DisplayTransform(view, UserLayer::Flag_DisplayHelp);
        if constexpr (D == D3)
        {
            const fvec3 lookDir = p_Data.Context->GetViewLookDirectionInCurrentAxes();
            ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", lookDir.x, lookDir.y, lookDir.z);
            UserLayer::HelpMarkerSameLine("The look direction is the direction the camera is facing. It is the "
                                          "direction of the camera's 'forward' vector in the current axes.");
        }
        if constexpr (D == D3)
        {
            i32 perspective = static_cast<i32>(p_Data.Perspective);
            if (ImGui::Combo("Projection", &perspective, "Orthographic\0Perspective\0\0"))
            {
                p_Data.Perspective = perspective == 1;
                if (p_Data.Perspective)
                    p_Data.Context->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
                else
                    p_Data.Context->SetOrthographicProjection();
            }

            if (p_Data.Perspective)
            {
                f32 degs = glm::degrees(p_Data.FieldOfView);

                bool changed = ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f);
                changed |= ImGui::SliderFloat("Near", &p_Data.Near, 0.1f, 10.f);
                changed |= ImGui::SliderFloat("Far", &p_Data.Far, 10.f, 100.f);
                if (changed)
                {
                    p_Data.FieldOfView = glm::radians(degs);
                    p_Data.Context->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
                }
            }
        }

        ImGui::Text("The camera/view controls are the following:");
        UserLayer::DisplayCameraMovementControls<D>();
        ImGui::TextWrapped(
            "The view describes the position and orientation of the camera in the scene. It is defined as a matrix "
            "that corresponds to the inverse of the camera's transform, and is applied to all objects in the scene. "
            "When you 'move' the camera around, you are actually moving the scene in the opposite direction. That is "
            "why the inverse is needed to transform the scene around you.");

        ImGui::TextWrapped("The projection is defined as an additional matrix that is applied on top of the view. It "
                           "projects and maps your scene onto your screen, and is responsible for the dimensions, "
                           "aspect ratio and, if using a 3D perspective, the field of view of the scene. In Onyx, only "
                           "orthographic and perspective projections are available. Orthographic projections are "
                           "embedded into the view's transform.");
        ImGui::TextWrapped("Orthographic projection: The scene is projected onto the screen without any perspective. "
                           "This means that objects do not get smaller as they move away from the camera. This is "
                           "useful for 2D games or when you want to keep the size of objects constant.");
        ImGui::TextWrapped("Perspective projection: The scene is projected onto the screen with perspective. This "
                           "means that objects get smaller as they move away from the camera, similar as how real life "
                           "vision behaves. This is useful for 3D games or when you want to create a sense of depth in "
                           "your scene. In Onyx, this projection is only available in 3D scenes.");
    }
}

void WindowData::renderLightSpawn() noexcept
{
    ImGui::SliderFloat("Ambient intensity", &m_LayerData3.Ambient.w, 0.f, 1.f);
    ImGui::ColorEdit3("Color", glm::value_ptr(m_LayerData3.Ambient));

    if (ImGui::Button("Spawn##Light"))
    {
        if (m_LayerData3.LightToSpawn == 0)
            m_LayerData3.DirectionalLights.push_back(m_LayerData3.DirLightToAdd);
        else
            m_LayerData3.PointLights.push_back(m_LayerData3.PointLightToAdd);
    }
    ImGui::SameLine();
    ImGui::Combo("Light", &m_LayerData3.LightToSpawn, "Directional\0Point\0\0");
    if (m_LayerData3.LightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &m_LayerData3.DrawLights);

    if ((!m_LayerData3.DirectionalLights.empty() || !m_LayerData3.PointLights.empty()) &&
        ImGui::TreeNode("Spawned lights"))
    {
        for (u32 i = 0; i < m_LayerData3.DirectionalLights.size(); ++i)
        {
            ImGui::PushID(&m_LayerData3.DirectionalLights[i]);
            if (ImGui::Button("X"))
            {
                m_LayerData3.DirectionalLights.erase(m_LayerData3.DirectionalLights.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            if (ImGui::Selectable("Directional", m_LayerData3.SelectedDirLight == i))
                m_LayerData3.SelectedDirLight = i;
            ImGui::PopID();
        }
        if (m_LayerData3.SelectedDirLight < m_LayerData3.DirectionalLights.size())
            UserLayer::DirectionalLightEditor(m_LayerData3.DirectionalLights[m_LayerData3.SelectedDirLight],
                                              UserLayer::Flag_DisplayHelp);

        for (u32 i = 0; i < m_LayerData3.PointLights.size(); ++i)
        {
            ImGui::PushID(&m_LayerData3.PointLights[i]);
            if (ImGui::Button("X"))
            {
                m_LayerData3.PointLights.erase(m_LayerData3.PointLights.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            if (ImGui::Selectable("Point", m_LayerData3.SelectedPointLight == i))
                m_LayerData3.SelectedPointLight = i;
            ImGui::PopID();
        }
        if (m_LayerData3.SelectedPointLight < m_LayerData3.PointLights.size())
            UserLayer::PointLightEditor(m_LayerData3.PointLights[m_LayerData3.SelectedPointLight],
                                        UserLayer::Flag_DisplayHelp);
        ImGui::TreePop();
    }
}

} // namespace Onyx::Demo