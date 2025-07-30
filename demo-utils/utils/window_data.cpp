#include "utils/window_data.hpp"
#include "glm/trigonometric.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/app/user_layer.hpp"
#include "utils/shapes.hpp"
#include "vkit/pipeline/pipeline_job.hpp"
#include "vkit/vulkan/vulkan.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include <imgui.h>
#include <implot.h>

// dirty macros as lazy enums lol
#define MESH 0
#define TRIANGLE 1
#define SQUARE 2
#define CIRCLE 3
#define NGON 4
#define POLYGON 5
#define STADIUM 6
#define ROUNDED_SQUARE 7
#define CUBE 8
#define SPHERE 9
#define CYLINDER 10
#define CAPSULE 11
#define ROUNDED_CUBE 12

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

void WindowData::OnStart(Window *p_Window, const Scene p_Scene) noexcept
{
    m_Window = p_Window;

    const auto presult =
        VKit::GraphicsPipeline::Builder(Core::GetDevice(), getRainbowLayout(), m_Window->CreateSceneRenderInfo())
            .SetViewportCount(1)
            .AddShaderStage(GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(getRainbowShader(), VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .AddDefaultColorAttachment()
            .Build();

    VKIT_ASSERT_RESULT(presult);
    const VKit::GraphicsPipeline &pipeline = presult.GetValue();

    const auto jresult = VKit::GraphicsJob::Create(presult.GetValue(), getRainbowLayout());
    VKIT_ASSERT_RESULT(jresult);
    m_RainbowJob = jresult.GetValue();

    VKit::PipelineLayout::Builder builder = m_Window->GetPostProcessing()->CreatePipelineLayoutBuilder();
    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    m_BlurLayout = result.GetValue();

    Core::GetDeletionQueue().SubmitForDeletion(pipeline);
    Core::GetDeletionQueue().SubmitForDeletion(m_BlurLayout);

    if (p_Scene == Scene::Setup2D)
    {
        ContextData<D2> &data = addContext<D2>(m_ContextData2);
        setupContext<D2>(data);
    }
    else if (p_Scene == Scene::Setup3D)
    {
        ContextData<D3> &data = addContext<D3>(m_ContextData3);
        setupContext<D3>(data);
    }
}

void WindowData::OnUpdate() noexcept
{
    if (!m_PostProcessing)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnUpdate");
    m_BlurData.Width = static_cast<f32>(m_Window->GetPixelWidth());
    m_BlurData.Height = static_cast<f32>(m_Window->GetPixelHeight());
    m_Window->GetPostProcessing()->UpdatePushConstantRange(0, &m_BlurData);
}

void WindowData::OnRender(const VkCommandBuffer p_CommandBuffer, const TKit::Timespan p_Timestep) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnRender");
    for (u32 i = 0; i < m_ContextData2.Data.GetSize(); ++i)
        drawShapes(m_ContextData2.Data[i], p_Timestep, m_ContextData2.Active && i == m_ContextData2.Selected);

    for (u32 i = 0; i < m_ContextData3.Data.GetSize(); ++i)
        drawShapes(m_ContextData3.Data[i], p_Timestep, m_ContextData3.Active && i == m_ContextData3.Selected);

    if (m_RainbowBackground)
    {
        m_RainbowJob.Bind(p_CommandBuffer);
        m_RainbowJob.Draw(p_CommandBuffer, 3);
    }
}

void WindowData::OnImGuiRender() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnImGuiRender");
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

    m_ContextData2.Active = ImGui::BeginTabItem("2D");
    if (m_ContextData2.Active)
    {
        renderUI(m_ContextData2);
        ImGui::EndTabItem();
    }
    m_ContextData3.Active = ImGui::BeginTabItem("3D");
    if (m_ContextData3.Active)
    {
        renderUI(m_ContextData3);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

template <Dimension D> static void processEvent(ContextDataContainer<D> &p_Container, const Event &p_Event)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    for (u32 i = 0; i < p_Container.Data.GetSize(); ++i)
        if (i == p_Container.Selected)
            for (u32 j = 0; j < p_Container.Data[i].Cameras.GetSize(); ++j)
                if (j == p_Container.Data[i].ActiveCamera)
                {
                    ContextData<D> &data = p_Container.Data[i];
                    Camera<D> *camera = data.Cameras[j].Camera;
                    if (p_Event.Type == Event::MousePressed && p_Container.Data[i].ShapeToSpawn == POLYGON)
                    {
                        data.PolygonVertices.Append(camera->GetWorldMousePosition());
                        data.Lattice.NeedsUpdate = true;
                    }

                    else if (p_Event.Type == Event::Scrolled)
                    {
                        const f32 factor = Input::IsKeyPressed(p_Event.Window, Input::Key::LeftShift) &&
                                                   !ImGui::GetIO().WantCaptureKeyboard
                                               ? 0.05f
                                               : 0.005f;
                        camera->ControlScrollWithUserInput(factor * p_Event.ScrollOffset.y);
                    }
                    return;
                }
}

void WindowData::OnEvent(const Event &p_Event) noexcept
{
    processEvent(m_ContextData2, p_Event);
    processEvent(m_ContextData3, p_Event);
}

static TKit::Array16<std::string> getEmptyStrings() noexcept
{
    TKit::Array16<std::string> customNames{};
    for (u32 i = 0; i < 16; ++i)
        customNames[i] = std::string{};
    return customNames;
}

template <Dimension D> static void renderMeshLoad(const char *p_Path)
{
    static Transform<D> transform{};
    static TKit::Array16<std::string> customNames = getEmptyStrings();

    const auto names = NamedMesh<D>::Query(p_Path);
    if (names.IsEmpty())
    {
        ImGui::TextDisabled("No meshes found at %s", p_Path);
        return;
    }

    UserLayer::TransformEditor(transform, UserLayer::Flag_DisplayHelp);

    ImGui::PushID(&transform);
    for (u32 i = 0; i < names.GetSize(); ++i)
    {
        const std::string &name = names[i];
        std::string &cname = customNames[i];
        if (cname.empty())
            cname = name;

        ImGui::Spacing();
        ImGui::Text("%s", name.c_str());
        constexpr u32 msize = 15;

        char input[msize + 1];
        std::strncpy(input, cname.c_str(), msize);
        input[msize] = '\0';

        ImGui::PushID(&name);
        if (ImGui::InputText("Mesh name", input, msize + 1))
            cname = input;

        // Should consider using fs::path
        ImGui::SameLine();
        const bool isLoaded = NamedMesh<D>::IsLoaded(cname);
        if (!isLoaded && ImGui::Button("Load"))
        {
            const auto result =
                NamedMesh<D>::Load(cname, std::string(p_Path) + "/" + name, transform.ComputeTransform());
            if (!result)
            {
                const std::string error = result.GetError().ToString();
                ImGui::Text("Failed to load mesh: %s. Cause: %s", name.c_str(), error.c_str());
            }
        }
        else if (isLoaded)
            ImGui::TextDisabled("Loaded");
        ImGui::PopID();
    }
    ImGui::PopID();
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

        const char *path2 = ONYX_ROOT_PATH "/demo-utils/meshes2/";
        const char *path3 = ONYX_ROOT_PATH "/demo-utils/meshes3/";
        ImGui::TextWrapped(
            "You may load meshes for this demo to use located in the '%s' and '%s' paths, for 2D and 3D "
            "meshes respectively. Take into account that meshes may have been created with a different coordinate "
            "system or unit scaling values. In Onyx, shapes with unit transforms are supposed to be centered around "
            "zero with a cartesian coordinate system and size (from end to end) of 1. That is why you may apply a "
            "transform before loading a specific mesh.",
            path2, path3);

        if (ImGui::CollapsingHeader("2D Meshes"))
            renderMeshLoad<D2>(path2);
        if (ImGui::CollapsingHeader("3D Meshes"))
            renderMeshLoad<D3>(path3);
    }
    ImGui::End();
}
void WindowData::RenderEditorText() noexcept
{
    ImGui::Text("This is the editor panel, where you can interact with the demo.");
    ImGui::TextWrapped("Onyx windows can draw shapes in 2D and 3D, and have a separate API for each even though the "
                       "window is shared. Users interact with the rendering API through rendering contexts.");
}

template <Dimension D>
void WindowData::drawShapes(const ContextData<D> &p_Data, const TKit::Timespan p_Timestep, const bool p_Active) noexcept
{
    p_Data.Context->Flush(m_BackgroundColor);
    if (p_Active)
        for (u32 i = 0; i < p_Data.Cameras.GetSize(); ++i)
            if (i == p_Data.ActiveCamera)
            {
                p_Data.Cameras[i].Camera->ControlMovementWithUserInput(p_Timestep);
                break;
            }
    p_Data.Context->TransformAxes(p_Data.AxesTransform.ComputeTransform());

    const LatticeData<D> &lattice = p_Data.Lattice;
    const uvec<D> &dims = lattice.Dimensions;
    if (lattice.Enabled && lattice.Shape)
    {
        const fvec<D> separation =
            lattice.PropToScale ? lattice.Shape->Transform.Scale * lattice.Separation : fvec<D>{lattice.Separation};
        const fvec<D> midPoint = 0.5f * separation * fvec<D>{dims - 1u};

        lattice.Shape->SetProperties(p_Data.Context);
        if (lattice.Multithreaded)
        {
            TKit::ITaskManager *tm = Core::GetTaskManager();
            using Task = TKit::Ref<TKit::Task<void>>;
            if constexpr (D == D2)
            {
                const u32 size = dims.x * dims.y;
                const auto fn = [&dims, &separation, &lattice, &midPoint, &p_Data](const u32 p_Start, const u32 p_End,
                                                                                   const u32) {
                    Transform<D2> transform = lattice.Shape->Transform;
                    for (u32 i = p_Start; i < p_End; ++i)
                    {
                        const u32 ix = i / dims.y;
                        const u32 iy = i % dims.y;
                        const f32 x = separation.x * static_cast<f32>(ix);
                        const u32 y = separation.y * static_cast<f32>(iy);
                        transform.Translation = fvec2{x, y} - midPoint;
                        lattice.Shape->DrawRaw(p_Data.Context, transform);
                    }
                };

                TKit::Array<Task, ONYX_MAX_THREADS> tasks{};
                TKit::ForEachMainThreadLead(*tm, 0u, size, tasks.begin(), lattice.Tasks, fn);
                for (u32 i = 0; i < lattice.Tasks - 1; ++i)
                    tasks[i]->WaitUntilFinished();
            }
            else
            {
                const u32 size = dims.x * dims.y * dims.z;
                const u32 size2 = dims.y * dims.z;
                const auto fn = [&dims, size2, &separation, &lattice, &midPoint, &p_Data](const u32 p_Start,
                                                                                          const u32 p_End, const u32) {
                    Transform<D3> transform = lattice.Shape->Transform;
                    for (u32 i = p_Start; i < p_End; ++i)
                    {
                        const u32 ix = i / size2;
                        const u32 iy = i / dims.y;
                        const u32 iz = i % dims.z;
                        const f32 x = separation.x * static_cast<f32>(ix);
                        const u32 y = separation.y * static_cast<f32>(iy);
                        const u32 z = separation.z * static_cast<f32>(iz);
                        transform.Translation = fvec3{x, y, z} - midPoint;
                        lattice.Shape->DrawRaw(p_Data.Context, transform);
                    }
                };
                TKit::Array<Task, ONYX_MAX_THREADS> tasks{};
                TKit::ForEachMainThreadLead(*tm, 0u, size, tasks.begin(), lattice.Tasks, fn);
                for (u32 i = 0; i < lattice.Tasks - 1; ++i)
                    tasks[i]->WaitUntilFinished();
            }
        }
        else
            for (u32 i = 0; i < dims.x; ++i)
            {
                const f32 x = static_cast<f32>(i) * separation.x;
                for (u32 j = 0; j < dims.y; ++j)
                {
                    const f32 y = static_cast<f32>(j) * separation.y;
                    if constexpr (D == D2)
                    {
                        lattice.Shape->Transform.Translation = fvec2{x, y} - midPoint;
                        lattice.Shape->DrawRaw(p_Data.Context);
                    }
                    else
                        for (u32 k = 0; k < dims.z; ++k)
                        {
                            const f32 z = static_cast<f32>(k) * separation.z;
                            lattice.Shape->Transform.Translation = fvec3{x, y, z} - midPoint;
                            lattice.Shape->DrawRaw(p_Data.Context);
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

template <typename C, typename F1, typename F2>
static void renderSelectableNoRemoval(const char *p_TreeName, C &p_Container, u32 &p_Selected, F1 &&p_OnSelected,
                                      F2 p_GetName) noexcept
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
                                   F2 &&p_OnRemoval) noexcept
{
    renderSelectable(nullptr, p_Container, p_Selected, std::forward<F1>(p_OnSelected), std::forward<F2>(p_OnRemoval),
                     [p_ElementName](const auto &) { return p_ElementName; });
}

template <typename C, typename F1, typename F2, typename F3>
static void renderSelectable(const char *p_TreeName, C &p_Container, u32 &p_Selected, F1 &&p_OnSelected,
                             F2 &&p_OnRemoval, F3 &&p_GetName) noexcept
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
            const char *name = std::forward<F3>(p_GetName)(p_Container[i]);
            if (ImGui::Selectable(name, i == p_Selected))
                p_Selected = i;

            ImGui::PopID();
        }
        if (p_Selected < p_Container.GetSize())
            std::forward<F1>(p_OnSelected)(p_Container[p_Selected]);
        if (p_TreeName)
            ImGui::TreePop();
    }
}

template <Dimension D> static void renderShapeSpawn(ContextData<D> &p_Data) noexcept
{
    const auto createShape = [&p_Data]() -> TKit::Scope<Shape<D>> {
        if (p_Data.ShapeToSpawn == MESH)
            return p_Data.Mesh.Mesh ? TKit::Scope<MeshShape<D>>::Create(p_Data.Mesh) : nullptr;
        else if (p_Data.ShapeToSpawn == TRIANGLE)
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
            if (p_Data.PolygonVertices.GetSize() < 3)
                return nullptr;

            auto polygon = TKit::Scope<Polygon<D>>::Create();
            polygon->Vertices = p_Data.PolygonVertices;
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

    const bool canSpawnPoly = p_Data.ShapeToSpawn != POLYGON || p_Data.PolygonVertices.GetSize() >= 3;
    const bool canSpawnMesh = p_Data.ShapeToSpawn != MESH || p_Data.Mesh.Mesh;
    if (!canSpawnPoly)
        ImGui::TextDisabled("A polygon must have at least 3 vertices to spawn!");
    else if (!canSpawnMesh)
        ImGui::TextDisabled("No valid mesh has been selected!");
    else if (ImGui::Button("Spawn##Shape"))
        p_Data.Shapes.Append(createShape());

    if (canSpawnPoly && canSpawnMesh)
        ImGui::SameLine();

    LatticeData<D> &lattice = p_Data.Lattice;
    if constexpr (D == D2)
        lattice.NeedsUpdate |=
            ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                         "Mesh\0Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0Rounded Square\0\0");
    else
        lattice.NeedsUpdate |= ImGui::Combo("Shape", &p_Data.ShapeToSpawn,
                                            "Mesh\0Triangle\0Square\0Circle\0NGon\0Polygon\0Stadium\0Rounded "
                                            "Square\0Cube\0Sphere\0Cylinder\0Capsule\0Rounded Cube\0\0");

    if (p_Data.ShapeToSpawn == MESH)
    {
        const auto &meshes = NamedMesh<D>::Get();
        if (!meshes.IsEmpty())
        {
            TKit::StaticArray16<const char *> meshNames{};
            for (const NamedMesh<D> &mesh : meshes)
                meshNames.Append(mesh.Name.c_str());
            lattice.NeedsUpdate |= ImGui::Combo("Mesh ID", &p_Data.MeshToSpawn, meshNames.GetData(),
                                                static_cast<i32>(meshNames.GetSize()));
            p_Data.Mesh = meshes[p_Data.MeshToSpawn];
        }
        else
            ImGui::TextDisabled("No meshes have been loaded yet! Load from the welcome window.");
    }
    else if (p_Data.ShapeToSpawn == NGON)
        lattice.NeedsUpdate |= ImGui::SliderInt("Sides", &p_Data.NGonSides, 3, ONYX_MAX_REGULAR_POLYGON_SIDES);
    else if (p_Data.ShapeToSpawn == POLYGON)
    {
        ImGui::Text("Vertices must be in counter clockwise order for outlines to work correctly");
        ImGui::Text("Click on the screen or the 'Add' button to add vertices to the polygon.");
        if (ImGui::Button("Clear"))
        {
            lattice.NeedsUpdate = true;
            p_Data.PolygonVertices.Clear();
        }

        lattice.NeedsUpdate |= ImGui::DragFloat2("Vertex", glm::value_ptr(p_Data.VertexToAdd), 0.1f);

        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            p_Data.PolygonVertices.Append(p_Data.VertexToAdd);
            p_Data.VertexToAdd = fvec2{0.f};
            lattice.NeedsUpdate = true;
        }
        for (u32 i = 0; i < p_Data.PolygonVertices.GetSize(); ++i)
        {
            ImGui::PushID(&p_Data.PolygonVertices[i]);
            if (ImGui::Button("X"))
            {
                p_Data.PolygonVertices.RemoveOrdered(p_Data.PolygonVertices.begin() + i);
                ImGui::PopID();
                lattice.NeedsUpdate = true;
                break;
            }

            ImGui::SameLine();
            ImGui::Text("Vertex %u: (%.2f, %.2f)", i, p_Data.PolygonVertices[i].x, p_Data.PolygonVertices[i].y);
            ImGui::PopID();
        }
    }
    if (lattice.Enabled && lattice.NeedsUpdate)
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
        ImGui::Checkbox("Multithreaded", &lattice.Multithreaded);
        if (lattice.Multithreaded)
            ImGui::SliderInt("Tasks", (int *)&lattice.Tasks, 1, ONYX_MAX_THREADS);

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
        if constexpr (D == D2)
        {
            if (line.Rounded)
                p_Data.Context->RoundedLine(line.Start, line.End, line.Thickness);
            else
                p_Data.Context->Line(line.Start, line.End, line.Thickness);
        }
        else
        {
            if (line.Rounded)
                p_Data.Context->RoundedLine(line.Start, line.End, {.Thickness = line.Thickness});
            else
                p_Data.Context->Line(line.Start, line.End, {.Thickness = line.Thickness});
        }
        p_Data.Context->Pop();
        ImGui::TreePop();
    }

    renderSelectableNoRemoval(
        "Shapes##Singular", p_Data.Shapes, p_Data.SelectedShape,
        [](const TKit::Scope<Shape<D>> &p_Shape) { p_Shape->Edit(); },
        [](const TKit::Scope<Shape<D>> &p_Shape) { return p_Shape->GetName(); });
}

template <Dimension D> void WindowData::renderCamera(CameraData<D> &p_Data) noexcept
{
    Camera<D> *camera = p_Data.Camera;
    const fvec2 vpos = camera->GetViewportMousePosition();
    ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos.x, vpos.y);

    if constexpr (D == D2)
    {
        const fvec2 wpos2 = camera->GetWorldMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f)", wpos2.x, wpos2.y);
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

        const fvec3 mpos3 = camera->GetWorldMousePosition(p_Data.ZOffset);
        const fvec2 vpos3 = camera->GetViewportMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f, %.2f)", mpos3.x, mpos3.y, mpos3.z);
    }
    UserLayer::HelpMarkerSameLine(
        "The world mouse position has world units, meaning it is scaled to the world "
        "coordinates of the current rendering context and are compatible with the translation units of the shapes.");

    ImGui::Checkbox("Transparent", &camera->Transparent);
    if (!camera->Transparent)
        ImGui::ColorEdit3("Background", camera->BackgroundColor.AsPointer());

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

    const Transform<D> view = camera->GetViewTransform();
    ImGui::Text("View transform (with respect current axes)");
    UserLayer::HelpMarkerSameLine(
        "This view transform is represented specifically with respect the current axes, "
        "but note that, as the view is a global state that is not reset every frame in "
        "a rendering context, it is generally detached from the axes transform. Onyx, under the hood, uses "
        "the detached view transform to setup the scene. This not a design decision but a requirement, as the axes "
        "is a somewhat volatile state (it is reset every frame).");

    UserLayer::DisplayTransform(view, UserLayer::Flag_DisplayHelp);
    if constexpr (D == D3)
    {
        const fvec3 lookDir = camera->GetViewLookDirection();
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
                camera->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
            else
                camera->SetOrthographicProjection();
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
                camera->SetPerspectiveProjection(p_Data.FieldOfView, p_Data.Near, p_Data.Far);
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

template <Dimension D> ContextData<D> &WindowData::addContext(ContextDataContainer<D> &p_Container) noexcept
{
    ContextData<D> &contextData = p_Container.Data.Append();
    contextData.Context = m_Window->CreateRenderContext<D>();
    return contextData;
}
template <Dimension D> void WindowData::setupContext(ContextData<D> &p_Data) noexcept
{
    CameraData<D> &cameraData = addCamera(p_Data);
    if constexpr (D == D3)
    {
        p_Data.DrawAxes = true;
        cameraData.Perspective = true;
        cameraData.Camera->SetPerspectiveProjection(cameraData.FieldOfView, cameraData.Near, cameraData.Far);
        Transform<D3> transform{};
        transform.Translation = {2.f, 0.75f, 2.f};
        transform.Rotation = glm::quat{glm::radians(fvec3{-15.f, 45.f, -4.f})};
        cameraData.Camera->SetView(transform);
        p_Data.DirectionalLights.Append(fvec4{1.f, 1.f, 1.f, 0.55f}, Color::WHITE);
    }
}
template <Dimension D> CameraData<D> &WindowData::addCamera(ContextData<D> &p_Data) noexcept
{
    Camera<D> *camera = p_Data.Context->CreateCamera();
    camera->BackgroundColor = Color{0.1f};

    CameraData<D> &camData = p_Data.Cameras.Append();
    camData.Camera = camera;
    return camData;
}

template <Dimension D> void WindowData::renderUI(ContextDataContainer<D> &p_Container) noexcept
{
    const fvec2 spos = Input::GetScreenMousePosition(m_Window);
    ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos.x, spos.y);
    UserLayer::HelpMarkerSameLine("The screen mouse position is always normalized to the window size, always ranging "
                                  "from -1 to 1 for 'x' and 'y', and from 0 to 1 for 'z'.");

    ImGui::Checkbox("Empty context", &p_Container.EmptyContext);
    UserLayer::HelpMarkerSameLine(
        "A rendering context is always initialized empty by default. But for convenience reasons, this demo will "
        "create "
        "contexts with a working camera and some other convenient settings enabled, unless this checkbox is marked.");

    if (ImGui::Button("Add context"))
    {
        ContextData<D> &data = addContext(p_Container);
        if (!p_Container.EmptyContext)
            setupContext(data);
    }

    UserLayer::HelpMarkerSameLine("A rendering context is an immediate mode API that allows users (you) to draw many "
                                  "different objects in a window. Multiple contexts may exist per window, each with "
                                  "their own independent state.");

    renderSelectableNoTree(
        "Context", p_Container.Data, p_Container.Selected, [this](ContextData<D> &p_Data) { renderUI(p_Data); },
        [this](const ContextData<D> &p_Data) { m_Window->DestroyRenderContext(p_Data.Context); });
}

template <Dimension D> void WindowData::renderUI(ContextData<D> &p_Data) noexcept
{
    if (p_Data.Cameras.IsEmpty())
        ImGui::TextDisabled("Context has no cameras. At least one must be added to render anything.");

    if (ImGui::CollapsingHeader("Shapes"))
        renderShapeSpawn(p_Data);
    if constexpr (D == D3)
        if (ImGui::CollapsingHeader("Lights"))
            renderLightSpawn(p_Data);

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

    if (ImGui::CollapsingHeader("Cameras"))
    {
        if (ImGui::Button("Add camera"))
            addCamera(p_Data);

        renderSelectableNoTree(
            "Camera", p_Data.Cameras, p_Data.ActiveCamera, [this](CameraData<D> &p_Camera) { renderCamera(p_Camera); },
            [&p_Data](const CameraData<D> &p_Camera) { p_Data.Context->DestroyCamera(p_Camera.Camera); });
    }
}

void WindowData::renderLightSpawn(ContextData<D3> &p_Data) noexcept
{
    ImGui::SliderFloat("Ambient intensity", &p_Data.Ambient.w, 0.f, 1.f);
    ImGui::ColorEdit3("Color", glm::value_ptr(p_Data.Ambient));

    if (ImGui::Button("Spawn##Light"))
    {
        if (p_Data.LightToSpawn == 0)
            p_Data.DirectionalLights.Append(fvec4{1.f, 1.f, 1.f, 0.55f}, Color::WHITE);
        else
            p_Data.PointLights.Append(fvec4{0.f, 0.f, 0.f, 1.f}, Color::WHITE, 1.f);
    }
    ImGui::SameLine();
    ImGui::Combo("Light", &p_Data.LightToSpawn, "Directional\0Point\0\0");
    if (p_Data.LightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &p_Data.DrawLights);

    renderSelectableNoRemoval(
        "Directional lights", p_Data.DirectionalLights, p_Data.SelectedDirLight,
        [](DirectionalLight &p_Light) { UserLayer::DirectionalLightEditor(p_Light); }, "Directional");

    renderSelectableNoRemoval(
        "Point lights", p_Data.PointLights, p_Data.SelectedPointLight,
        [](PointLight &p_Light) { UserLayer::PointLightEditor(p_Light); }, "Point");
}

} // namespace Onyx::Demo
