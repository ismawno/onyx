#include "sbox/layer.hpp"
#include "onyx/app/user_layer.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/imgui/implot.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/dialog.hpp"
#include "onyx/property/camera.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx::Demo
{
// this readds the meshes everytime. so it could blow up haha but idc
template <Dimension D> void addMeshes(MeshContainer &meshes)
{
    const auto add = [&meshes](const char *name, const StatMeshData<D> &mesh) {
        const Mesh mesh = Assets::AddMesh(mesh);
        meshes.StaticMeshes.Append(MeshId{name, mesh});
    };
    add("Triangle", Assets::CreateTriangleMesh<D>());
    add("Square", Assets::CreateSquareMesh<D>());
    if constexpr (D == D3)
    {
        add("Cube", Assets::CreateCubeMesh());
        add("Sphere", Assets::CreateSphereMesh(32, 64));
        add("Cylinder", Assets::CreateCylinderMesh(64));
        meshes.StaticOffset = 5;
    }
    else
        meshes.StaticOffset = 2;
    Assets::Upload<D>();
}

SandboxLayer::SandboxLayer(Application *application, Window *window, const Dimension dim)
    : UserLayer(application, window)
{
    if (dim == D2)
    {
        ContextData<D2> &context = addContext(m_ContextData2);
        setupContext<D2>(context);
        addCamera(m_Cameras2);
    }
    else if (dim == D3)
    {
        ContextData<D3> &context = addContext(m_ContextData3);
        setupContext<D3>(context);
        CameraData<D3> camera = addCamera(m_Cameras3);
        setupCamera(camera);
    }
    addMeshes<D2>(m_Meshes2);
    addMeshes<D3>(m_Meshes3);
}

void SandboxLayer::OnFrameBegin(const DeltaTime &deltaTime, const FrameInfo &)
{
    const TKit::Timespan ts = deltaTime.Measured;
    TKIT_PROFILE_NSCOPE("Onyx::Demo::OnFrameBegin");

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
template <Dimension D> static void loadMesh(MeshContainer &meshes, const Dialog::Path &path)
{
    const auto result = Assets::LoadStaticMesh<D>(path.c_str());
    if (!result)
        return;

    const StatMeshData<D> &data = result.GetValue();
    const Mesh mesh = Assets::AddMesh(data);
    Assets::Upload<D>();

    const std::string name = path.filename().string();
    meshes.StaticMeshes.Append(MeshId{name, mesh});
}
template <Dimension D> static void renderMeshLoad(MeshContainer &meshes, const char *default)
{
    ImGui::PushID(default);
    if (ImGui::Button("Load"))
    {
        const auto result = Dialog::OpenSingle({.Default = default});
        if (result)
            loadMesh<D>(meshes, result.GetValue());
    }
    for (u32 i = meshes.StaticOffset; i < meshes.StaticMeshes.GetSize(); ++i)
        ImGui::BulletText("Static mesh: %s", meshes.StaticMeshes[i].Name.c_str());
    ImGui::PopID();
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
                    m_Application->OpenWindow({.CreationCallback = [this](Window *window) {
                        m_Application->SetUserLayer<SandboxLayer>(window, D2);
                    }});

                if (ImGui::MenuItem("3D"))
                    m_Application->OpenWindow({.CreationCallback = [this](Window *window) {
                        m_Application->SetUserLayer<SandboxLayer>(window, D3);
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
        m_Application->DisplayDeltaTime(m_Window, UserLayerFlag_DisplayHelp);
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
        UserLayer::PresentModeEditor(m_Window, UserLayerFlag_DisplayHelp);

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
static void renderSelectableNoRemoval(const char *treeName, C &container, u32 &selected, F1 &&onSelected, F2 getName)
{
    if constexpr (std::is_same_v<F2, const char *>)
        renderSelectable(
            treeName, container, selected, std::forward<F1>(onSelected), [](const auto &) {},
            [getName](const auto &) { return getName; });
    else
        renderSelectable(treeName, container, selected, std::forward<F1>(onSelected), [](const auto &) {}, getName);
}

template <typename C, typename F1, typename F2>
static void renderSelectableNoTree(const char *elementName, C &container, u32 &selected, F1 &&onSelected,
                                   F2 &&onRemoval)
{
    renderSelectable(nullptr, container, selected, std::forward<F1>(onSelected), std::forward<F2>(onRemoval),
                     [elementName](const auto &) { return elementName; });
}

template <typename C, typename F1, typename F2, typename F3>
static void renderSelectable(const char *treeName, C &container, u32 &selected, F1 &&onSelected, F2 &&onRemoval,
                             F3 &&getName)
{
    if (!container.IsEmpty() && (!treeName || ImGui::TreeNode(treeName)))
    {
        for (u32 i = 0; i < container.GetSize(); ++i)
        {
            ImGui::PushID(&container[i]);
            if (ImGui::Button("X"))
            {
                std::forward<F2>(onRemoval)(container[i]);
                container.RemoveOrdered(container.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            const std::string name = std::forward<F3>(getName)(container[i]);
            if (ImGui::Selectable(name.c_str(), i == selected))
                selected = i;

            ImGui::PopID();
        }
        if (selected < container.GetSize())
            std::forward<F1>(onSelected)(container[selected]);
        if (treeName)
            ImGui::TreePop();
    }
}

template <typename... Args> bool combo(const char *name, u32 *index, Args &&...args)
{
    i32 index = static_cast<i32>(*index);
    if (ImGui::Combo(name, &index, std::forward<Args>(args)...))
    {
        *index = static_cast<u32>(index);
        return true;
    }
    return false;
}

template <Dimension D> void editShape(Shape<D> &shape)
{
    ImGui::PushID(&shape);
    ImGui::Text("Transform");
    ImGui::SameLine();
    UserLayer::TransformEditor<D>(shape.Transform, UserLayerFlag_DisplayHelp);
    ImGui::Text("Material");
    ImGui::SameLine();
    UserLayer::MaterialEditor<D>(shape.Material, UserLayerFlag_DisplayHelp);
    ImGui::Checkbox("Fill", &shape.Fill);
    ImGui::Checkbox("Outline", &shape.Outline);
    ImGui::SliderFloat("Outline Width", &shape.OutlineWidth, 0.01f, 0.1f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::ColorEdit4("Outline Color", shape.OutlineColor.GetData());
    if (shape.Type == ShapeType<D>::Circle)
    {
        ImGui::SliderFloat("Inner Fade", &shape.CircleOptions.InnerFade, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Outer Fade", &shape.CircleOptions.OuterFade, 0.f, 1.f, "%.2f");
        ImGui::SliderAngle("Lower Angle", &shape.CircleOptions.LowerAngle);
        ImGui::SliderAngle("Upper Angle", &shape.CircleOptions.UpperAngle);
        ImGui::SliderFloat("Hollowness", &shape.CircleOptions.Hollowness, 0.f, 1.f, "%.2f");
    }
    ImGui::PopID();
}
template <Dimension D> void setShapeProperties(RenderContext<D> *context, const Shape<D> &shape)
{
    context->Material(shape.Material);
    context->OutlineWidth(shape.OutlineWidth);
    context->Outline(shape.OutlineColor);
    context->Fill(shape.Fill);
    context->Outline(shape.Outline);
}
template <Dimension D>
void drawShape(RenderContext<D> *context, const Shape<D> &shape, const Transform<D> *transform = nullptr)
{
    if (shape.Type == ShapeType<D>::Circle)
        context->Circle((transform ? *transform : shape.Transform).ComputeTransform(), shape.CircleOptions);
    else
        context->StaticMesh(shape.Mesh, (transform ? *transform : shape.Transform).ComputeTransform());
}

template <Dimension D> static void renderShapeSpawn(MeshContainer &meshes, ContextData<D> &context)
{
    const auto createShape = [&]() -> Shape<D> {
        Shape<D> shape{};
        shape.Type = static_cast<ShapeType<D>>(context.ShapeToSpawn);
        if (context.ShapeToSpawn == u32(ShapeType<D>::ImportedStatic))
        {
            const MeshId &mesh = meshes.StaticMeshes[meshes.StaticOffset + context.ImportedStatToSpawn];
            shape.Name = mesh.Name;
            shape.Mesh = mesh.Mesh;
        }
        else if (context.ShapeToSpawn == u32(ShapeType<D>::Circle))
            shape.Name = "Circle";
        else
        {
            const MeshId &mesh = meshes.StaticMeshes[context.ShapeToSpawn];
            shape.Name = mesh.Name;
            shape.Mesh = mesh.Mesh;
        }

        return shape;
    };
    const auto isBadSpawnImportedStatic = [&] {
        return context.ShapeToSpawn == u32(ShapeType<D>::ImportedStatic) &&
               context.ImportedStatToSpawn + meshes.StaticOffset >= meshes.StaticMeshes.GetSize();
    };

    if (isBadSpawnImportedStatic())
        ImGui::TextDisabled("No valid mesh has been selected!");

    else if (ImGui::Button("Spawn##Shape"))
        context.Shapes.Append(createShape());

    if (!isBadSpawnImportedStatic())
        ImGui::SameLine();

    LatticeData<D> &lattice = context.Lattice;
    if constexpr (D == D2)
        lattice.NeedsUpdate |=
            combo("Shape", &context.ShapeToSpawn, "Triangle\0Square\0Imported static meshes\0Circle\0\0");
    else
        lattice.NeedsUpdate |= combo("Shape", &context.ShapeToSpawn,
                                     "Triangle\0Square\0Cube\0Sphere\0Cylinder\0Imported static meshes\0Circle\0\0");

    if (context.ShapeToSpawn == u32(ShapeType<D>::ImportedStatic))
    {
        if (meshes.StaticMeshes.GetSize() > meshes.StaticOffset)
        {
            TKit::StaticArray16<const char *> meshNames{};
            for (u32 i = meshes.StaticOffset; i < meshes.StaticMeshes.GetSize(); ++i)
                meshNames.Append(meshes.StaticMeshes[i].Name.c_str());

            lattice.NeedsUpdate |= combo("Mesh ID", &context.ImportedStatToSpawn, meshNames.GetData(),
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

        ImGui::Text("Lattice shape:");
        editShape(lattice.Shape);

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Line test"))
    {
        LineTest<D> &line = context.Line;

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
        UserLayer::MaterialEditor<D>(line.Material, UserLayerFlag_DisplayHelp);
        ImGui::ColorEdit3("Outline color", line.OutlineColor.GetData());

        context.Context->Push();
        if (line.Outline)
        {
            context.Context->Outline(line.OutlineColor);
            context.Context->OutlineWidth(line.OutlineWidth);
        }
        context.Context->Material(line.Material);
        if constexpr (D == D2)
            context.Context->Line(meshes.StaticMeshes[u32(ShapeType<D2>::Square)].Mesh, line.Start, line.End,
                                  line.Thickness);
        else
            context.Context->Line(meshes.StaticMeshes[u32(ShapeType<D3>::Cylinder)].Mesh, line.Start, line.End,
                                  line.Thickness);
        context.Context->Pop();
        ImGui::TreePop();
    }

    renderSelectableNoRemoval(
        "Shapes##Singular", context.Shapes, context.SelectedShape, [](Shape<D> &shape) { editShape<D>(shape); },
        [](const Shape<D> &shape) { return shape.Name; });
}

template <Dimension D> void SandboxLayer::renderCamera(CameraData<D> &camera)
{
    Camera<D> *camera = camera.Camera;
    const f32v2 vpos = camera->GetViewportMousePosition();
    ImGui::Text("Viewport mouse position: (%.2f, %.2f)", vpos[0], vpos[1]);

    if constexpr (D == D2)
    {
        const f32v2 wpos2 = camera->GetWorldMousePosition();
        ImGui::Text("World mouse position: (%.2f, %.2f)", wpos2[0], wpos2[1]);
    }
    else
    {
        ImGui::SliderFloat("Mouse Z offset", &camera.ZOffset, 0.f, 1.f);
        UserLayer::HelpMarkerSameLine(
            "In 3D, the world mouse position can be ambiguous because of the extra dimension. This amibiguity needs to "
            "somehow be resolved. In most use-cases, ray casting is the best approach to fully define this position, "
            "but because this is a simple demo, the z offset can be manually specified, and is in the range [0, 1] "
            "(screen coordinates). Note that, if in perspective mode, 0 corresponds to the near plane and 1 to the "
            "far plane.");

        const f32v3 mpos3 = camera->GetWorldMousePosition(camera.ZOffset);
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
    if (UserLayer::ViewportEditor(viewport, UserLayerFlag_DisplayHelp))
        camera->SetViewport(viewport);

    ImGui::Text("Scissor");
    ImGui::SameLine();
    ScreenScissor scissor = camera->GetScissor();
    if (UserLayer::ScissorEditor(scissor, UserLayerFlag_DisplayHelp))
        camera->SetScissor(scissor);

    const Transform<D> &view = camera->GetProjectionViewData().View;
    ImGui::Text("View transform");
    UserLayer::HelpMarkerSameLine(
        "The view transform are the coordinates of the camera, detached from any render context coordinate system.");

    UserLayer::DisplayTransform(view, UserLayerFlag_DisplayHelp);
    if constexpr (D == D3)
    {
        const f32v3 lookDir = camera->GetViewLookDirection();
        ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", lookDir[0], lookDir[1], lookDir[2]);
        UserLayer::HelpMarkerSameLine("The look direction is the direction the camera is facing. It is the "
                                      "direction of the camera's 'forward' vector.");
    }
    if constexpr (D == D3)
    {
        i32 perspective = static_cast<i32>(camera.Perspective);
        if (ImGui::Combo("Projection", &perspective, "Orthographic\0Perspective\0\0"))
        {
            camera.Perspective = perspective == 1;
            if (camera.Perspective)
                camera->SetPerspectiveProjection(camera.FieldOfView, camera.Near, camera.Far);
            else
                camera->SetOrthographicProjection();
        }

        if (camera.Perspective)
        {
            f32 degs = Math::Degrees(camera.FieldOfView);

            bool changed = ImGui::SliderFloat("Field of view", &degs, 75.f, 90.f);
            changed |= ImGui::SliderFloat("Near", &camera.Near, 0.1f, 10.f);
            changed |= ImGui::SliderFloat("Far", &camera.Far, 10.f, 100.f);
            if (changed)
            {
                camera.FieldOfView = Math::Radians(degs);
                camera->SetPerspectiveProjection(camera.FieldOfView, camera.Near, camera.Far);
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
template <Dimension D> void SandboxLayer::renderCameras(CameraDataContainer<D> &cameras)
{
    if (ImGui::CollapsingHeader("Cameras"))
    {
        if (cameras.Cameras.IsEmpty())
            ImGui::TextDisabled(
                "Window has no cameras for this dimension. At least one must be added to render anything 2D.");

        if (ImGui::Button("Add camera"))
            addCamera(cameras);

        renderSelectableNoTree(
            "Camera", cameras.Cameras, cameras.Active, [this](CameraData<D> &camera) { renderCamera(camera); },
            [this](const CameraData<D> &camera) { m_Window->DestroyCamera(camera.Camera); });
    }
}

template <Dimension D> void SandboxLayer::renderUI(ContextDataContainer<D> &contexts)
{
    const f32v2 spos = Input::GetScreenMousePosition(m_Window);
    ImGui::Text("Screen mouse position: (%.2f, %.2f)", spos[0], spos[1]);
    UserLayer::HelpMarkerSameLine(
        "The screen mouse position is always Math::Normalized to the window size, always ranging "
        "from -1 to 1 for 'x' and 'y', and from 0 to 1 for 'z'.");

    ImGui::Checkbox("Empty context", &contexts.EmptyContext);
    UserLayer::HelpMarkerSameLine(
        "A rendering context is always initialized empty by default. But for convenience reasons, this demo will "
        "create "
        "contexts with a working camera and some other convenient settings enabled, unless this checkbox is marked.");

    if (ImGui::Button("Add context"))
    {
        ContextData<D> &data = addContext(contexts);
        if (!contexts.EmptyContext)
            setupContext(data);
    }

    UserLayer::HelpMarkerSameLine("A rendering context is an immediate mode API that allows users (you) to draw many "
                                  "different objects in a window. Multiple contexts may exist per window, each with "
                                  "their own independent state.");

    renderSelectableNoTree(
        "Context", contexts.Contexts, contexts.Active, [this](ContextData<D> &context) { renderUI(context); },
        [this](const ContextData<D> &context) { m_Window->DestroyRenderContext(context.Context); });
}

template <Dimension D> void SandboxLayer::renderUI(ContextData<D> &context)
{

    if (ImGui::CollapsingHeader("Shapes"))
        renderShapeSpawn(D == D2 ? m_Meshes2 : m_Meshes3, context);
    if constexpr (D == D3)
        if (ImGui::CollapsingHeader("Lights"))
            renderLightSpawn(context);

    if (ImGui::CollapsingHeader("Axes"))
    {
        ImGui::Checkbox("Draw##Axes", &context.DrawAxes);
        if (context.DrawAxes)
            ImGui::SliderFloat("Axes thickness", &context.AxesThickness, 0.001f, 0.1f);

        if (ImGui::TreeNode("Material"))
        {
            ImGui::SameLine();
            UserLayer::MaterialEditor<D>(context.AxesMaterial, UserLayerFlag_DisplayHelp);
            ImGui::TreePop();
        }
    }
}

void SandboxLayer::renderLightSpawn(ContextData<D3> &context)
{
    ImGui::SliderFloat("Ambient intensity", &context.Ambient[3], 0.f, 1.f);
    ImGui::ColorEdit3("Color", Math::AsPointer(context.Ambient));

    if (ImGui::Button("Spawn##Light"))
    {
        if (context.LightToSpawn == 0)
            context.DirectionalLights.Append(f32v3{1.f, 1.f, 1.f}, 0.3f, Color::WHITE.Pack());
        else
            context.PointLights.Append(f32v3{0.f, 0.f, 0.f}, 0.3f, 1.f, Color::WHITE.Pack());
    }
    ImGui::SameLine();
    ImGui::Combo("Light", &context.LightToSpawn, "Directional\0Point\0\0");
    if (context.LightToSpawn == 1)
        ImGui::Checkbox("Draw##Light", &context.DrawLights);

    renderSelectableNoRemoval(
        "Directional lights", context.DirectionalLights, context.SelectedDirLight,
        [](DirectionalLight &light) { UserLayer::DirectionalLightEditor(light); }, "Directional");

    renderSelectableNoRemoval(
        "Point lights", context.PointLights, context.SelectedPointLight,
        [](PointLight &light) { UserLayer::PointLightEditor(light); }, "Point");
}
#endif

template <Dimension D>
static void processEvent(Window *window, ContextDataContainer<D> &contexts, const CameraDataContainer<D> &cameras,
                         const Event &event)
{
#ifdef ONYX_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
        return;
#endif
    if (cameras.Cameras.IsEmpty() || contexts.Contexts.IsEmpty())
        return;

    const CameraData<D> &cam = cameras.Cameras[cameras.Active];
    Camera<D> *camera = cam.Camera;
    if (event.Type == Event_Scrolled)
    {
        const f32 factor = Input::IsKeyPressed(window, Input::Key_LeftShift) ? 0.05f : 0.005f;
        camera->ControlScrollWithUserInput(factor * event.ScrollOffset[1]);
    }
}

void SandboxLayer::OnEvent(const Event &event)
{
    processEvent(m_Window, m_ContextData2, m_Cameras2, event);
    processEvent(m_Window, m_ContextData3, m_Cameras3, event);
}

template <Dimension D> void SandboxLayer::drawShapes(const ContextData<D> &context)
{
    context.Context->Flush();

    const LatticeData<D> &lattice = context.Lattice;
    const u32v<D> &dims = lattice.Dimensions;
    if (lattice.Enabled && lattice.Shape.Mesh != NullMesh)
    {
        const f32v<D> separation =
            lattice.PropToScale ? lattice.Shape.Transform.Scale * lattice.Separation : f32v<D>{lattice.Separation};
        const f32v<D> midPoint = 0.5f * separation * f32v<D>{dims - 1u};

        setShapeProperties(context.Context, lattice.Shape);
        context.Context->ShareCurrentState();

        TKit::ITaskManager *tm = Core::GetTaskManager();
        if constexpr (D == D2)
        {
            const u32 size = dims[0] * dims[1];
            const auto fn = [&](const u32 start, const u32 end) {
                Transform<D2> transform = lattice.Shape.Transform;
                for (u32 i = start; i < end; ++i)
                {
                    const u32 ix = i / dims[1];
                    const u32 iy = i % dims[1];
                    const f32 x = separation[0] * static_cast<f32>(ix);
                    const f32 y = separation[1] * static_cast<f32>(iy);
                    transform.Translation = f32v2{x, y} - midPoint;
                    drawShape(context.Context, lattice.Shape, &transform);
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
            const auto fn = [&, yz](const u32 start, const u32 end) {
                Transform<D3> transform = lattice.Shape.Transform;
                for (u32 i = start; i < end; ++i)
                {
                    const u32 ix = i / yz;
                    const u32 j = ix * yz;
                    const u32 iy = (i - j) / dims[2];
                    const u32 iz = (i - j) % dims[2];
                    const f32 x = separation[0] * static_cast<f32>(ix);
                    const f32 y = separation[1] * static_cast<f32>(iy);
                    const f32 z = separation[2] * static_cast<f32>(iz);
                    transform.Translation = f32v3{x, y, z} - midPoint;
                    drawShape(context.Context, lattice.Shape, &transform);
                }
            };
            TKit::FixedArray<Task, MaxTasks> tasks{};
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), lattice.Partitions, fn);

            const u32 tcount = (lattice.Partitions - 1) >= MaxTasks ? MaxTasks : (lattice.Partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
                tm->WaitUntilFinished(tasks[i]);
        }
    }

    for (const Shape<D> &shape : context.Shapes)
    {
        setShapeProperties(context.Context, shape);
        drawShape(context.Context, shape);
    }

    context.Context->Outline(false);
    if (context.DrawAxes)
    {
        context.Context->Material(context.AxesMaterial);
        context.Context->Fill();
        if constexpr (D == D2)
            context.Context->Axes(m_Meshes2.StaticMeshes[u32(ShapeType<D2>::Square)].Mesh,
                                  {.Thickness = context.AxesThickness});
        else
            context.Context->Axes(m_Meshes3.StaticMeshes[u32(ShapeType<D3>::Cylinder)].Mesh,
                                  {.Thickness = context.AxesThickness});
    }

    if constexpr (D == D3)
    {
        context.Context->AmbientColor(context.Ambient);
        for (const auto &light : context.DirectionalLights)
            context.Context->DirectionalLight(light);
        for (const auto &light : context.PointLights)
        {
            if (context.DrawLights)
            {
                context.Context->Push();
                context.Context->Fill(Color::Unpack(light.Color));
                context.Context->Scale(0.01f);
                context.Context->Translate(light.Position);
                context.Context->StaticMesh(m_Meshes3.StaticMeshes[u32(ShapeType<D3>::Sphere)].Mesh);
                context.Context->Pop();
            }
            context.Context->PointLight(light);
        }
    }
}

template <Dimension D> ContextData<D> &SandboxLayer::addContext(ContextDataContainer<D> &contexts)
{
    ContextData<D> &contextData = contexts.Contexts.Append();
    contextData.Context = m_Window->CreateRenderContext<D>();
    return contextData;
}
template <Dimension D> void SandboxLayer::setupContext(ContextData<D> &context)
{
    if constexpr (D == D3)
    {
        context.DrawAxes = true;
        context.DirectionalLights.Append(f32v3{1.f, 1.f, 1.f}, 0.3f, Color::WHITE.Pack());
    }
}
template <Dimension D> CameraData<D> &SandboxLayer::addCamera(CameraDataContainer<D> &cameras)
{
    Camera<D> *camera = m_Window->CreateCamera<D>();
    camera->BackgroundColor = Color{0.1f};

    CameraData<D> &camData = cameras.Cameras.Append();
    camData.Camera = camera;
    return camData;
}
void SandboxLayer::setupCamera(CameraData<D3> &camera)
{
    camera.Perspective = true;
    camera.Camera->SetPerspectiveProjection(camera.FieldOfView, camera.Near, camera.Far);
    Transform<D3> transform{};
    transform.Translation = f32v3{2.f, 0.75f, 2.f};
    transform.Rotation = f32q{Math::Radians(f32v3{-15.f, 45.f, -4.f})};
    camera.Camera->SetView(transform);
}

} // namespace Onyx::Demo
