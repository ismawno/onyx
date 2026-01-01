#include "sbox/layer.hpp"
#include "onyx/app/user_layer.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/imgui/implot.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/dialog.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/property/camera.hpp"
#include "vkit/state/pipeline_job.hpp"
#include "vkit/vulkan/vulkan.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx::Demo
{
static const VKit::PipelineLayout &getRainbowLayout()
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
static const VKit::Shader &getRainbowShader()
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = Pipelines::CreateShader(ONYX_ROOT_PATH "/demo/shaders/rainbow.frag");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}

template <Dimension D> void addMeshes(MeshContainer &p_Meshes)
{
    const auto add = [&p_Meshes](const char *p_Name, const StatMeshData<D> &p_Mesh) {
        const Mesh mesh = Assets::AddMesh(p_Mesh);
        p_Meshes.StaticMeshes.Append(MeshId{p_Name, mesh});
    };
    add("Triangle", Assets::CreateTriangleMesh<D>());
    add("Square", Assets::CreateSquareMesh<D>());
    if constexpr (D == D3)
    {
        add("Cube", Assets::CreateCubeMesh());
        add("Sphere", Assets::CreateSphereMesh(32, 64));
        add("Cylinder", Assets::CreateCylinderMesh(64));
        p_Meshes.StaticOffset = 5;
    }
    else
        p_Meshes.StaticOffset = 2;
    Assets::Upload<D>();
}

SandboxLayer::SandboxLayer(Application *p_Application, Window *p_Window, const Dimension p_Dim)
    : UserLayer(p_Application, p_Window)
{
    FrameScheduler *fs = m_Window->GetFrameScheduler();
    const auto presult =
        VKit::GraphicsPipeline::Builder(Core::GetDevice(), getRainbowLayout(), fs->CreateSceneRenderInfo())
            .SetViewportCount(1)
            .AddShaderStage(Pipelines::GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(getRainbowShader(), VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .AddDefaultColorAttachment()
            .Bake()
            .Build();

    VKIT_ASSERT_RESULT(presult);
    const VKit::GraphicsPipeline &pipeline = presult.GetValue();

    const auto jresult = VKit::GraphicsJob::Create(presult.GetValue(), getRainbowLayout());
    VKIT_ASSERT_RESULT(jresult);
    m_RainbowJob = jresult.GetValue();

    VKit::PipelineLayout::Builder builder = fs->GetPostProcessing()->CreatePipelineLayoutBuilder();
    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    m_BlurLayout = result.GetValue();

    Core::GetDeletionQueue().SubmitForDeletion(pipeline);
    Core::GetDeletionQueue().SubmitForDeletion(m_BlurLayout);
    if (p_Dim == D2)
    {
        ContextData<D2> &context = addContext(m_ContextData2);
        setupContext<D2>(context);
        addCamera(m_Cameras2);
    }
    else if (p_Dim == D3)
    {
        ContextData<D3> &context = addContext(m_ContextData3);
        setupContext<D3>(context);
        CameraData<D3> camera = addCamera(m_Cameras3);
        setupCamera(camera);
    }
    addMeshes<D2>(m_Meshes2);
    addMeshes<D3>(m_Meshes3);
}

void SandboxLayer::OnFrameBegin(const DeltaTime &p_DeltaTime, const FrameInfo &)
{
    const TKit::Timespan ts = p_DeltaTime.Measured;
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnFrameBegin");
    if (m_PostProcessing)
    {
        m_BlurData.Width = static_cast<f32>(m_Window->GetPixelWidth());
        m_BlurData.Height = static_cast<f32>(m_Window->GetPixelHeight());
        m_Window->GetFrameScheduler()->GetPostProcessing()->UpdatePushConstantRange(0, &m_BlurData);
    }

    if (!m_Cameras2.Cameras.IsEmpty())
        m_Cameras2.Cameras[m_Cameras2.Active].Camera->ControlMovementWithUserInput(ts);
    if (!m_Cameras3.Cameras.IsEmpty())
        m_Cameras3.Cameras[m_Cameras3.Active].Camera->ControlMovementWithUserInput(ts);

    for (u32 i = 0; i < m_ContextData2.Contexts.GetSize(); ++i)
        drawShapes(m_ContextData2.Contexts[i]);

    for (u32 i = 0; i < m_ContextData3.Contexts.GetSize(); ++i)
        drawShapes(m_ContextData3.Contexts[i]);

#ifdef ONYX_ENABLE_IMGUI
    renderImGui();
#endif
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> static void loadMesh(MeshContainer &p_Meshes, const Dialog::Path &p_Path)
{
    const auto result = Assets::LoadStaticMesh<D>(p_Path.string());
    if (!result)
        return;

    const StatMeshData<D> &data = result.GetValue();
    const Mesh mesh = Assets::AddMesh(data);
    Assets::Upload<D>();

    const std::string name = p_Path.filename().string();
    p_Meshes.StaticMeshes.Append(MeshId{name, mesh});
}
template <Dimension D> static void renderMeshLoad(MeshContainer &p_Meshes, const char *p_Default)
{
    ImGui::PushID(p_Default);
    if (ImGui::Button("Load"))
    {
        const auto result = Dialog::OpenSingle({.Default = p_Default});
        if (result)
            loadMesh<D>(p_Meshes, result.GetValue());
    }
    for (u32 i = p_Meshes.StaticOffset; i < p_Meshes.StaticMeshes.GetSize(); ++i)
        ImGui::BulletText("Static mesh: %s", p_Meshes.StaticMeshes[i].Name.c_str());
    ImGui::PopID();
}

static const VKit::Shader &getBlurShader()
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = Pipelines::CreateShader(ONYX_ROOT_PATH "/demo/shaders/blur.frag");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}

void SandboxLayer::renderImGui()
{
    ImGui::ShowDemoWindow();
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::ShowDemoWindow();
#    endif

    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnImGuiRender");
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("App"))
        {
#    ifdef __ONYX_MULTI_WINDOW
            if (ImGui::BeginMenu("New"))
            {
                if (ImGui::MenuItem("2D"))
                    m_Application->OpenWindow({.CreationCallback = [this](Window *p_Window) {
                        m_Application->SetUserLayer<SandboxLayer>(p_Window, D2);
                    }});

                if (ImGui::MenuItem("3D"))
                    m_Application->OpenWindow({.CreationCallback = [this](Window *p_Window) {
                        m_Application->SetUserLayer<SandboxLayer>(p_Window, D3);
                    }});

                ImGui::EndMenu();
            }
#    endif
            if (ImGui::MenuItem("Reload ImGui"))
                m_Application->ReloadImGui(m_Window);

            if (ImGui::MenuItem("Quit"))

#    ifdef __ONYX_MULTI_WINDOW
                m_Application->CloseWindow(m_Window);
#    else
                m_Application->Quit();
#    endif

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Welcome to Onyx, my Vulkan application framework!"))
    {
        m_Application->DisplayDeltaTime(m_Window, UserLayer::Flag_DisplayHelp);
        const TKit::Timespan ts = m_Application->GetDeltaTime();
        ImGui::Text("Application delta time: %.2f ms", ts.AsMilliseconds());

        ImGui::Text("Version: " ONYX_VERSION);
        ImGui::TextWrapped(
            "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
            "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
            "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
            "The framework is still in its early stages, but I plan to expand it further in the future.");

        ImGui::TextWrapped("This program is the Onyx demo, showcasing some of its features. Most of them can be tried "
                           "in the 'Editor' panel.");

        ImGui::TextLinkOpenURL("My GitHub", "https://github.com/ismawno");

        ImGui::TextWrapped(
            "You may load meshes for this demo to use for both 2D and 3D. Take into account that meshes may "
            "have been created with a different coordinate "
            "system or unit scaling values. In Onyx, shapes with unit transforms are supposed to be centered around "
            "zero with a cartesian coordinate system and size (from end to end) of 1. That is why you may apply a "
            "transform before loading a specific mesh.");

        if (ImGui::CollapsingHeader("2D Meshes"))
            renderMeshLoad<D2>(m_Meshes2, ONYX_ROOT_PATH "/demo/meshes2/");
        if (ImGui::CollapsingHeader("3D Meshes"))
            renderMeshLoad<D3>(m_Meshes3, ONYX_ROOT_PATH "/demo/meshes3/");
    }
    ImGui::End();

    if (ImGui::Begin("Editor"))
    {
        ImGui::Text("This is the editor panel, where you can interact with the demo.");
        ImGui::TextWrapped(
            "Onyx windows can draw shapes in 2D and 3D, and have a separate API for each even though the "
            "window is shared. Users interact with the rendering API through rendering contexts.");
        UserLayer::PresentModeEditor(m_Window, UserLayer::Flag_DisplayHelp);

        ImGui::Checkbox("Rainbow background", &m_RainbowBackground);
        UserLayer::HelpMarkerSameLine("This is a small demonstration of how to hook-up your own pipelines to the Onyx "
                                      "rendering context (in this case, to draw a nice rainbow background).");

        if (ImGui::Checkbox("Blur", &m_PostProcessing))
        {
            FrameScheduler *fs = m_Window->GetFrameScheduler();
            if (m_PostProcessing)
            {
                m_BlurData.Width = static_cast<f32>(m_Window->GetPixelWidth());
                m_BlurData.Height = static_cast<f32>(m_Window->GetPixelHeight());
                fs->SetPostProcessing(m_BlurLayout, getBlurShader())->UpdatePushConstantRange(0, &m_BlurData);
            }
            else
                fs->RemovePostProcessing();
        }
        UserLayer::HelpMarkerSameLine("This is a small demonstration of how to hook-up a post-processing pipeline to "
                                      "the Onyx rendering context to "
                                      "apply transformations to the final image (in this case, a blur effect).");

        if (m_PostProcessing)
        {
            const u32 mn = 0;
            const u32 mx = 12;
            ImGui::SliderScalar("Blur kernel size", ImGuiDataType_U32, &m_BlurData.KernelSize, &mn, &mx);
        }

        ImGui::BeginTabBar("Dimension");

        if (ImGui::BeginTabItem("2D"))
        {
            renderCameras(m_Cameras2);
            renderUI(m_ContextData2);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("3D"))
        {
            renderCameras(m_Cameras3);
            renderUI(m_ContextData3);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
template <typename C, typename F1, typename F2>
static void renderSelectableNoRemoval(const char *p_TreeName, C &p_Container, u32 &p_Selected, F1 &&p_OnSelected,
                                      F2 p_GetName)
{
    if constexpr (std::is_same_v<F2, const char *>)
        renderSelectable(
            p_TreeName, p_Container, p_Selected, std::forward<F1>(p_OnSelected), [](const auto &) {},
            [p_GetName](const auto &) { return p_GetName; });
    else
        renderSelectable(
            p_TreeName, p_Container, p_Selected, std::forward<F1>(p_OnSelected), [](const auto &) {}, p_GetName);
}

template <typename C, typename F1, typename F2>
static void renderSelectableNoTree(const char *p_ElementName, C &p_Container, u32 &p_Selected, F1 &&p_OnSelected,
                                   F2 &&p_OnRemoval)
{
    renderSelectable(nullptr, p_Container, p_Selected, std::forward<F1>(p_OnSelected), std::forward<F2>(p_OnRemoval),
                     [p_ElementName](const auto &) { return p_ElementName; });
}

template <typename C, typename F1, typename F2, typename F3>
static void renderSelectable(const char *p_TreeName, C &p_Container, u32 &p_Selected, F1 &&p_OnSelected,
                             F2 &&p_OnRemoval, F3 &&p_GetName)
{
    if (!p_Container.IsEmpty() && (!p_TreeName || ImGui::TreeNode(p_TreeName)))
    {
        for (u32 i = 0; i < p_Container.GetSize(); ++i)
        {
            ImGui::PushID(&p_Container[i]);
            if (ImGui::Button("X"))
            {
                std::forward<F2>(p_OnRemoval)(p_Container[i]);
                p_Container.RemoveOrdered(p_Container.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            const std::string name = std::forward<F3>(p_GetName)(p_Container[i]);
            if (ImGui::Selectable(name.c_str(), i == p_Selected))
                p_Selected = i;

            ImGui::PopID();
        }
        if (p_Selected < p_Container.GetSize())
            std::forward<F1>(p_OnSelected)(p_Container[p_Selected]);
        if (p_TreeName)
            ImGui::TreePop();
    }
}

template <typename... Args> bool combo(const char *p_Name, u32 *p_Index, Args &&...p_Args)
{
    i32 index = static_cast<i32>(*p_Index);
    if (ImGui::Combo(p_Name, &index, std::forward<Args>(p_Args)...))
    {
        *p_Index = static_cast<u32>(index);
        return true;
    }
    return false;
}

template <Dimension D> void editShape(Shape<D> &p_Shape)
{
    ImGui::PushID(&p_Shape);
    ImGui::Text("Transform");
    ImGui::SameLine();
    UserLayer::TransformEditor<D>(p_Shape.Transform, UserLayer::Flag_DisplayHelp);
    ImGui::Text("Material");
    ImGui::SameLine();
    UserLayer::MaterialEditor<D>(p_Shape.Material, UserLayer::Flag_DisplayHelp);
    ImGui::Checkbox("Fill", &p_Shape.Fill);
    ImGui::Checkbox("Outline", &p_Shape.Outline);
    ImGui::SliderFloat("Outline Width", &p_Shape.OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::ColorEdit4("Outline Color", p_Shape.OutlineColor.GetData());
    if (p_Shape.Type == Shape_Circle)
    {
        ImGui::SliderFloat("Inner Fade", &p_Shape.CircleOptions.InnerFade, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Outer Fade", &p_Shape.CircleOptions.OuterFade, 0.f, 1.f, "%.2f");
        ImGui::SliderAngle("Lower Angle", &p_Shape.CircleOptions.LowerAngle);
        ImGui::SliderAngle("Upper Angle", &p_Shape.CircleOptions.UpperAngle);
        ImGui::SliderFloat("Hollowness", &p_Shape.CircleOptions.Hollowness, 0.f, 1.f, "%.2f");
    }
    ImGui::PopID();
}
template <Dimension D> void setShapeProperties(RenderContext<D> *p_Context, const Shape<D> &p_Shape)
{
    p_Context->Material(p_Shape.Material);
    p_Context->OutlineWidth(p_Shape.OutlineWidth);
    p_Context->Outline(p_Shape.OutlineColor);
    p_Context->Fill(p_Shape.Fill);
    p_Context->Outline(p_Shape.Outline);
}
template <Dimension D>
void drawShape(RenderContext<D> *p_Context, const Shape<D> &p_Shape, const Transform<D> *p_Transform = nullptr)
{
    if (p_Shape.Type == Shape_Circle)
        p_Context->Circle((p_Transform ? *p_Transform : p_Shape.Transform).ComputeTransform(), p_Shape.CircleOptions);
    else
        p_Context->StaticMesh(p_Shape.Mesh, (p_Transform ? *p_Transform : p_Shape.Transform).ComputeTransform());
}

template <Dimension D> static void renderShapeSpawn(MeshContainer &p_Meshes, ContextData<D> &p_Context)
{
    const auto createShape = [&]() -> Shape<D> {
        Shape<D> shape{};
        shape.Type = static_cast<ShapeType>(p_Context.ShapeToSpawn);
        if (p_Context.ShapeToSpawn == Shape_ImportedStatic)
        {
            const MeshId &mesh = p_Meshes.StaticMeshes[p_Meshes.StaticOffset + p_Context.ImportedStatToSpawn];
            shape.Name = mesh.Name;
            shape.Mesh = mesh.Mesh;
        }
        else if (p_Context.ShapeToSpawn == Shape_Circle)
            shape.Name = "Circle";
        else
        {
            const MeshId &mesh = p_Meshes.StaticMeshes[p_Context.ShapeToSpawn];
            shape.Name = mesh.Name;
            shape.Mesh = mesh.Mesh;
        }

        return shape;
    };
    const auto isBadSpawnImportedStatic = [&] {
        return p_Context.ShapeToSpawn == Shape_ImportedStatic &&
               p_Context.ImportedStatToSpawn + p_Meshes.StaticOffset >= p_Meshes.StaticMeshes.GetSize();
    };

    if (isBadSpawnImportedStatic())
        ImGui::TextDisabled("No valid mesh has been selected!");

    else if (ImGui::Button("Spawn##Shape"))
        p_Context.Shapes.Append(createShape());

    if (!isBadSpawnImportedStatic())
        ImGui::SameLine();

    LatticeData<D> &lattice = p_Context.Lattice;
    if constexpr (D == D2)
        lattice.NeedsUpdate |=
            combo("Shape", &p_Context.ShapeToSpawn, "Triangle\0Square\0Imported static meshes\0Circle\0\0");
    else
        lattice.NeedsUpdate |= combo("Shape", &p_Context.ShapeToSpawn,
                                     "Triangle\0Square\0Cube\0Sphere\0Cylinder\0Imported static meshes\0Circle\0\0");

    if (p_Context.ShapeToSpawn == Shape_ImportedStatic)
    {
        if (p_Meshes.StaticMeshes.GetSize() > p_Meshes.StaticOffset)
        {
            TKit::Array16<const char *> meshNames{};
            for (u32 i = p_Meshes.StaticOffset; i < p_Meshes.StaticMeshes.GetSize(); ++i)
                meshNames.Append(p_Meshes.StaticMeshes[i].Name.c_str());

            lattice.NeedsUpdate |= combo("Mesh ID", &p_Context.ImportedStatToSpawn, meshNames.GetData(),
                                         static_cast<i32>(meshNames.GetSize()));
        }
        else
            ImGui::TextDisabled("No meshes have been loaded yet! Load from the welcome window.");
    }
    if (lattice.Enabled && lattice.NeedsUpdate && !isBadSpawnImportedStatic())
    {
        lattice.Shape = createShape();
        lattice.NeedsUpdate = false;
    }

    if (ImGui::TreeNode("Lattice"))
    {
        lattice.NeedsUpdate |= ImGui::Checkbox("Draw shape lattice", &lattice.Enabled);
        UserLayer::HelpMarkerSameLine("You may choose to draw a lattice of shapes to stress test the rendering engine. "
                                      "I advice to build the engine "
                                      "in distribution mode to see meaningful results.");
        const u32 mnt = 1;
        const u32 mxt = MaxThreads;
        ImGui::SliderScalar("Partitions", ImGuiDataType_U32, &lattice.Partitions, &mnt, &mxt);

        if constexpr (D == D2)
        {
            ImGui::Text("Shape count: %u", lattice.Dimensions[0] * lattice.Dimensions[1]);
            const u32 mn = 1;
            const u32 mx = TKIT_U32_MAX;
            ImGui::DragScalarN("Lattice dimensions", ImGuiDataType_U32, Math::AsPointer(lattice.Dimensions), 2, 2.f,
                               &mn, &mx);
        }
        else
        {
            ImGui::Text("Shape count: %u", lattice.Dimensions[0] * lattice.Dimensions[1] * lattice.Dimensions[2]);
            const u32 mn = 1;
            const u32 mx = TKIT_U32_MAX;
            ImGui::DragScalarN("Lattice dimensions", ImGuiDataType_U32, Math::AsPointer(lattice.Dimensions), 3, 2.f,
                               &mn, &mx);
        }

        ImGui::Checkbox("Separation proportional to scale", &lattice.PropToScale);
        ImGui::DragFloat("Lattice separation", &lattice.Separation, 0.01f, 0.f, FLT_MAX);
        if (lattice.Shape.Type != NullMesh)
        {
            ImGui::Text("Lattice shape:");
            editShape(lattice.Shape);
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Line test"))
    {
        LineTest<D> &line = p_Context.Line;

        ImGui::Checkbox("Outline", &line.Outline);
        ImGui::SliderFloat("Outline width", &line.OutlineWidth, 0.01f, 0.1f);
        ImGui::SliderFloat("Thickness", &line.Thickness, 0.01f, 0.1f);

        if constexpr (D == D2)
        {
            ImGui::DragFloat2("Start", Math::AsPointer(line.Start), 0.1f);
            ImGui::DragFloat2("End", Math::AsPointer(line.End), 0.1f);
        }
        else
        {
            ImGui::DragFloat3("Start", Math::AsPointer(line.Start), 0.1f);
            ImGui::DragFloat3("End", Math::AsPointer(line.End), 0.1f);
        }

        ImGui::Text("Material");
        UserLayer::MaterialEditor<D>(line.Material, UserLayer::Flag_DisplayHelp);
        ImGui::ColorEdit3("Outline color", line.OutlineColor.GetData());

        p_Context.Context->Push();
        if (line.Outline)
        {
            p_Context.Context->Outline(line.OutlineColor);
            p_Context.Context->OutlineWidth(line.OutlineWidth);
        }
        p_Context.Context->Material(line.Material);
        if constexpr (D == D2)
            p_Context.Context->Line(p_Meshes.StaticMeshes[Shape_Square].Mesh, line.Start, line.End, line.Thickness);
        else
            p_Context.Context->Line(p_Meshes.StaticMeshes[Shape_Cylinder].Mesh, line.Start, line.End, line.Thickness);
        p_Context.Context->Pop();
        ImGui::TreePop();
    }

    renderSelectableNoRemoval(
        "Shapes##Singular", p_Context.Shapes, p_Context.SelectedShape, [](Shape<D> &p_Shape) { editShape<D>(p_Shape); },
        [](const Shape<D> &p_Shape) { return p_Shape.Name; });
}

template <Dimension D> void SandboxLayer::renderCamera(CameraData<D> &p_Camera)
{
    Camera<D> *camera = p_Camera.Camera;
    const f32v2 vpos = camera->GetViewportMousePosition();
    ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos[0], vpos[1]);

    if constexpr (D == D2)
    {
        const f32v2 wpos2 = camera->GetWorldMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f)", wpos2[0], wpos2[1]);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &p_Camera.ZOffset, 0.f, 1.f);
        UserLayer::HelpMarkerSameLine(
            "In 3D, the world mouse position can be ambiguous because of the extra dimension. This amibiguity needs to "
            "somehow be resolved. In most use-cases, ray casting is the best approach to fully define this position, "
            "but because this is a simple demo, the z offset can be manually specified, and is in the range [0, 1] "
            "(screen coordinates). Note that, if in perspective mode, 0 corresponds to the near plane and 1 to the "
            "far plane.");

        const f32v3 mpos3 = camera->GetWorldMousePosition(p_Camera.ZOffset);
        const f32v2 vpos2 = camera->GetViewportMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f, %.2f)", mpos3[0], mpos3[1], mpos3[2]);
        ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos2[0], vpos2[1]);
    }
    UserLayer::HelpMarkerSameLine("The world mouse position has world units, meaning it takes into account the "
                                  "transform of the camera to compute the mouse coordinates. It will not, however, "
                                  "take into account the axes of any render context by default.");

    ImGui::Checkbox("Transparent", &camera->Transparent);
    if (!camera->Transparent)
        ImGui::ColorEdit3("Background", camera->BackgroundColor.GetData());

    ImGui::Text("Viewport");
    ImGui::SameLine();
    ScreenViewport viewport = camera->GetViewport();
    if (UserLayer::ViewportEditor(viewport, UserLayer::Flag_DisplayHelp))
        camera->SetViewport(viewport);

    ImGui::Text("Scissor");
    ImGui::SameLine();
    ScreenScissor scissor = camera->GetScissor();
    if (UserLayer::ScissorEditor(scissor, UserLayer::Flag_DisplayHelp))
        camera->SetScissor(scissor);

    const Transform<D> &view = camera->GetProjectionViewData().View;
    ImGui::Text("View transform");
    UserLayer::HelpMarkerSameLine(
        "The view transform are the coordinates of the camera, detached from any render context coordinate system.");

    UserLayer::DisplayTransform(view, UserLayer::Flag_DisplayHelp);
    if constexpr (D == D3)
    {
        const f32v3 lookDir = camera->GetViewLookDirection();
        ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", lookDir[0], lookDir[1], lookDir[2]);
        UserLayer::HelpMarkerSameLine("The look direction is the direction the camera is facing. It is the "
                                      "direction of the camera's 'forward' vector.");
    }
    if constexpr (D == D3)
    {
        i32 perspective = static_cast<i32>(p_Camera.Perspective);
        if (ImGui::Combo("Projection", &perspective, "Orthographic\0Perspective\0\0"))
        {
            p_Camera.Perspective = perspective == 1;
            if (p_Camera.Perspective)
                camera->SetPerspectiveProjection(p_Camera.FieldOfView, p_Camera.Near, p_Camera.Far);
            else
                camera->SetOrthographicProjection();
        }

        if (p_Camera.Perspective)
        {
            f32 degs = Math::Degrees(p_Camera.FieldOfView);

            bool changed = ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f);
            changed |= ImGui::SliderFloat("Near", &p_Camera.Near, 0.1f, 10.f);
            changed |= ImGui::SliderFloat("Far", &p_Camera.Far, 10.f, 100.f);
            if (changed)
            {
                p_Camera.FieldOfView = Math::Radians(degs);
                camera->SetPerspectiveProjection(p_Camera.FieldOfView, p_Camera.Near, p_Camera.Far);
            }
        }
    }

    ImGui::Text("The camera/view controls are the following:");
    UserLayer::DisplayCameraControls<D>();
    ImGui::TextWrapped(
        "The view describes the position and orientation of a camera in the scene. It is defined as a matrix "
        "that corresponds to the inverse of the camera's transform, and is applied to all objects in a context. "
        "When you 'move' a camera around, you are actually moving the scene (rendered by that camera) in the opposite "
        "direction. That is why the inverse is needed to transform the scene around you.");

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
template <Dimension D> void SandboxLayer::renderCameras(CameraDataContainer<D> &p_Cameras)
{
    if (ImGui::CollapsingHeader("Cameras"))
    {
        if (p_Cameras.Cameras.IsEmpty())
            ImGui::TextDisabled(
                "Window has no cameras for this dimension. At least one must be added to render anything 2D.");

        if (ImGui::Button("Add camera"))
            addCamera(p_Cameras);

        renderSelectableNoTree(
            "Camera", p_Cameras.Cameras, p_Cameras.Active, [this](CameraData<D> &p_Camera) { renderCamera(p_Camera); },
            [this](const CameraData<D> &p_Camera) { m_Window->DestroyCamera(p_Camera.Camera); });
    }
}

template <Dimension D> void SandboxLayer::renderUI(ContextDataContainer<D> &p_Contexts)
{
    const f32v2 spos = Input::GetScreenMousePosition(m_Window);
    ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos[0], spos[1]);
    UserLayer::HelpMarkerSameLine(
        "The screen mouse position is always Math::Normalized to the window size, always ranging "
        "from -1 to 1 for 'x' and 'y', and from 0 to 1 for 'z'.");

    ImGui::Checkbox("Empty context", &p_Contexts.EmptyContext);
    UserLayer::HelpMarkerSameLine(
        "A rendering context is always initialized empty by default. But for convenience reasons, this demo will "
        "create "
        "contexts with a working camera and some other convenient settings enabled, unless this checkbox is marked.");

    if (ImGui::Button("Add context"))
    {
        ContextData<D> &data = addContext(p_Contexts);
        if (!p_Contexts.EmptyContext)
            setupContext(data);
    }

    UserLayer::HelpMarkerSameLine("A rendering context is an immediate mode API that allows users (you) to draw many "
                                  "different objects in a window. Multiple contexts may exist per window, each with "
                                  "their own independent state.");

    renderSelectableNoTree(
        "Context", p_Contexts.Contexts, p_Contexts.Active, [this](ContextData<D> &p_Context) { renderUI(p_Context); },
        [this](const ContextData<D> &p_Context) { m_Window->DestroyRenderContext(p_Context.Context); });
}

template <Dimension D> void SandboxLayer::renderUI(ContextData<D> &p_Context)
{

    if (ImGui::CollapsingHeader("Shapes"))
        renderShapeSpawn(D == D2 ? m_Meshes2 : m_Meshes3, p_Context);
    if constexpr (D == D3)
        if (ImGui::CollapsingHeader("Lights"))
            renderLightSpawn(p_Context);

    if (ImGui::CollapsingHeader("Axes"))
    {
        ImGui::Checkbox("Draw##Axes", &p_Context.DrawAxes);
        if (p_Context.DrawAxes)
            ImGui::SliderFloat("Axes thickness", &p_Context.AxesThickness, 0.001f, 0.1f);

        if (ImGui::TreeNode("Material"))
        {
            ImGui::SameLine();
            UserLayer::MaterialEditor<D>(p_Context.AxesMaterial, UserLayer::Flag_DisplayHelp);
            ImGui::TreePop();
        }
    }
}

void SandboxLayer::renderLightSpawn(ContextData<D3> &p_Context)
{
    ImGui::SliderFloat("Ambient intensity", &p_Context.Ambient[3], 0.f, 1.f);
    ImGui::ColorEdit3("Color", Math::AsPointer(p_Context.Ambient));

    if (ImGui::Button("Spawn##Light"))
    {
        if (p_Context.LightToSpawn == 0)
            p_Context.DirectionalLights.Append(f32v3{1.f, 1.f, 1.f}, 0.3f, Color::WHITE.Pack());
        else
            p_Context.PointLights.Append(f32v3{0.f, 0.f, 0.f}, 0.3f, 1.f, Color::WHITE.Pack());
    }
    ImGui::SameLine();
    ImGui::Combo("Light", &p_Context.LightToSpawn, "Directional\0Point\0\0");
    if (p_Context.LightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &p_Context.DrawLights);

    renderSelectableNoRemoval(
        "Directional lights", p_Context.DirectionalLights, p_Context.SelectedDirLight,
        [](DirectionalLight &p_Light) { UserLayer::DirectionalLightEditor(p_Light); }, "Directional");

    renderSelectableNoRemoval(
        "Point lights", p_Context.PointLights, p_Context.SelectedPointLight,
        [](PointLight &p_Light) { UserLayer::PointLightEditor(p_Light); }, "Point");
}
#endif

template <Dimension D>
static void processEvent(Window *p_Window, ContextDataContainer<D> &p_Contexts, const CameraDataContainer<D> &p_Cameras,
                         const Event &p_Event)
{
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        return;
#endif
    if (p_Cameras.Cameras.IsEmpty() || p_Contexts.Contexts.IsEmpty())
        return;

    const CameraData<D> &cam = p_Cameras.Cameras[p_Cameras.Active];
    Camera<D> *camera = cam.Camera;
    if (p_Event.Type == Event_Scrolled)
    {
        const f32 factor = Input::IsKeyPressed(p_Window, Input::Key_LeftShift) ? 0.05f : 0.005f;
        camera->ControlScrollWithUserInput(factor * p_Event.ScrollOffset[1]);
    }
}

void SandboxLayer::OnEvent(const Event &p_Event)
{
    processEvent(m_Window, m_ContextData2, m_Cameras2, p_Event);
    processEvent(m_Window, m_ContextData3, m_Cameras3, p_Event);
}

void SandboxLayer::OnRenderBegin(const DeltaTime &, const FrameInfo &p_Info)
{
    if (m_RainbowBackground)
    {
        m_RainbowJob.Bind(p_Info.GraphicsCommand);
        m_RainbowJob.Draw(p_Info.GraphicsCommand, 3);
    }
}

template <Dimension D> void SandboxLayer::drawShapes(const ContextData<D> &p_Context)
{
    p_Context.Context->Flush();

    const LatticeData<D> &lattice = p_Context.Lattice;
    const u32v<D> &dims = lattice.Dimensions;
    if (lattice.Enabled && lattice.Shape.Mesh != NullMesh)
    {
        const f32v<D> separation =
            lattice.PropToScale ? lattice.Shape.Transform.Scale * lattice.Separation : f32v<D>{lattice.Separation};
        const f32v<D> midPoint = 0.5f * separation * f32v<D>{dims - 1u};

        setShapeProperties(p_Context.Context, lattice.Shape);
        p_Context.Context->ShareCurrentState();

        TKit::ITaskManager *tm = Core::GetTaskManager();
        if constexpr (D == D2)
        {
            const u32 size = dims[0] * dims[1];
            const auto fn = [&](const u32 p_Start, const u32 p_End) {
                Transform<D2> transform = lattice.Shape.Transform;
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / dims[1];
                    const u32 iy = i % dims[1];
                    const f32 x = separation[0] * static_cast<f32>(ix);
                    const f32 y = separation[1] * static_cast<f32>(iy);
                    transform.Translation = f32v2{x, y} - midPoint;
                    drawShape(p_Context.Context, lattice.Shape, &transform);
                }
            };

            TKit::FixedArray<Task, MaxTasks> tasks{};
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), lattice.Partitions, fn);

            const u32 tcount = (lattice.Partitions - 1) >= MaxTasks ? MaxTasks : (lattice.Partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
                tm->WaitUntilFinished(tasks[i]);
        }
        else
        {
            const u32 size = dims[0] * dims[1] * dims[2];
            const u32 yz = dims[1] * dims[2];
            const auto fn = [&, yz](const u32 p_Start, const u32 p_End) {
                Transform<D3> transform = lattice.Shape.Transform;
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / yz;
                    const u32 j = ix * yz;
                    const u32 iy = (i - j) / dims[2];
                    const u32 iz = (i - j) % dims[2];
                    const f32 x = separation[0] * static_cast<f32>(ix);
                    const f32 y = separation[1] * static_cast<f32>(iy);
                    const f32 z = separation[2] * static_cast<f32>(iz);
                    transform.Translation = f32v3{x, y, z} - midPoint;
                    drawShape(p_Context.Context, lattice.Shape, &transform);
                }
            };
            TKit::FixedArray<Task, MaxTasks> tasks{};
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), lattice.Partitions, fn);

            const u32 tcount = (lattice.Partitions - 1) >= MaxTasks ? MaxTasks : (lattice.Partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
                tm->WaitUntilFinished(tasks[i]);
        }
    }

    for (const Shape<D> &shape : p_Context.Shapes)
    {
        setShapeProperties(p_Context.Context, shape);
        drawShape(p_Context.Context, shape);
    }

    p_Context.Context->Outline(false);
    if (p_Context.DrawAxes)
    {
        p_Context.Context->Material(p_Context.AxesMaterial);
        p_Context.Context->Fill();
        if constexpr (D == D2)
            p_Context.Context->Axes(m_Meshes2.StaticMeshes[Shape_Square].Mesh, {.Thickness = p_Context.AxesThickness});
        else
            p_Context.Context->Axes(m_Meshes3.StaticMeshes[Shape_Cylinder].Mesh,
                                    {.Thickness = p_Context.AxesThickness});
    }

    if constexpr (D == D3)
    {
        p_Context.Context->AmbientColor(p_Context.Ambient);
        for (const auto &light : p_Context.DirectionalLights)
            p_Context.Context->DirectionalLight(light);
        for (const auto &light : p_Context.PointLights)
        {
            if (p_Context.DrawLights)
            {
                p_Context.Context->Push();
                p_Context.Context->Fill(Color::Unpack(light.Color));
                p_Context.Context->Scale(0.01f);
                p_Context.Context->Translate(light.Position);
                p_Context.Context->StaticMesh(m_Meshes3.StaticMeshes[Shape_Sphere].Mesh);
                p_Context.Context->Pop();
            }
            p_Context.Context->PointLight(light);
        }
    }
}

template <Dimension D> ContextData<D> &SandboxLayer::addContext(ContextDataContainer<D> &p_Contexts)
{
    ContextData<D> &contextData = p_Contexts.Contexts.Append();
    contextData.Context = m_Window->CreateRenderContext<D>();
    return contextData;
}
template <Dimension D> void SandboxLayer::setupContext(ContextData<D> &p_Context)
{
    if constexpr (D == D3)
    {
        p_Context.DrawAxes = true;
        p_Context.DirectionalLights.Append(f32v3{1.f, 1.f, 1.f}, 0.3f, Color::WHITE.Pack());
    }
}
template <Dimension D> CameraData<D> &SandboxLayer::addCamera(CameraDataContainer<D> &p_Cameras)
{
    Camera<D> *camera = m_Window->CreateCamera<D>();
    camera->BackgroundColor = Color{0.1f};

    CameraData<D> &camData = p_Cameras.Cameras.Append();
    camData.Camera = camera;
    return camData;
}
void SandboxLayer::setupCamera(CameraData<D3> &p_Camera)
{
    p_Camera.Perspective = true;
    p_Camera.Camera->SetPerspectiveProjection(p_Camera.FieldOfView, p_Camera.Near, p_Camera.Far);
    Transform<D3> transform{};
    transform.Translation = f32v3{2.f, 0.75f, 2.f};
    transform.Rotation = f32q{Math::Radians(f32v3{-15.f, 45.f, -4.f})};
    p_Camera.Camera->SetView(transform);
}

} // namespace Onyx::Demo
