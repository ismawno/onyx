#pragma once

#include "onyx/application/layer.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx
{
using SandboxFlags = u32;
enum SandboxFlagBit : SandboxFlags
{
    SandboxFlag_Fill = 1 << 0,
    SandboxFlag_Outline = 1 << 1,
    SandboxFlag_DrawLights = 1 << 2,
    SandboxFlag_DrawAxes = 1 << 3,
};

template <Dimension D> struct CameraData
{
    Camera<D> *Camera;
};

template <> struct CameraData<D3>
{
    Camera<D3> *Camera;
    f32 FieldOfView = Math::Radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
    f32 ZOffset = 0.f;

    bool Perspective = false;
};

template <Dimension D> struct Cameras
{
    TKit::TierArray<CameraData<D>> Cameras{};
    u32 Active = 0;
};

struct MeshId
{
    std::string Name;
    Mesh Mesh;
};

enum StaticMeshType : u8
{
    StaticMesh_Triangle,
    StaticMesh_Square,
    StaticMesh_Cube,
    StaticMesh_Sphere,
    StaticMesh_Cylinder,
    StaticMesh_Count,
};

struct ShapeType
{
    Geometry Geo;
    u32 StaticMesh;
};

template <Dimension D> struct Shape
{
    ShapeType Type;
    std::string Name;
    Mesh Mesh = NullMesh;
    Transform<D> Transform{};
    CircleOptions CircleOptions{};
    SandboxFlags Flags = SandboxFlag_Fill;
    Color FillColor = Color::White;
    Color OutlineColor = Color::Orange;
    f32 OutlineWidth = 0.01f;
};

template <Dimension D> struct ContextData
{
    RenderContext<D> *Context;
    TKit::TierArray<Shape<D>> Shapes;
    u32 GeometryToSpawn = Geometry_Circle;
    u32 StatMeshToSpawn = 0;

    u32 SelectedShape = 0;
    f32 AxesThickness = 0.01f;

    SandboxFlags Flags = 0;
};

template <> struct ContextData<D3>
{
    RenderContext<D3> *Context;
    TKit::TierArray<Shape<D3>> Shapes;
    u32 GeometryToSpawn = Geometry_Circle;
    u32 StatMeshToSpawn = 0;
    u32 SelectedShape = 0;
    f32 AxesThickness = 0.01f;

    TKit::TierArray<DirectionalLight> DirLights{};
    TKit::TierArray<PointLight> PointLights{};

    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};
    u32 LightToSpawn = 0;
    u32 SelectedDirLight = 0;
    u32 SelectedPointLight = 0;

    SandboxFlags Flags = 0;
};

template <Dimension D> struct Contexts
{
    TKit::TierArray<ContextData<D>> Contexts{};
    u32 Active = 0;
};

template <Dimension D> struct Meshes
{
    TKit::TierArray<MeshId> StaticMeshes{};
    u32 GeoToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};
};

template <> struct Meshes<D3>
{
    TKit::TierArray<MeshId> StaticMeshes{};
    u32 GeoToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};

    u32 Rings = 32;
    u32 Sectors = 64;
    u32 CylinderSides = 64;
};

class SandboxAppLayer final : public ApplicationLayer
{
  public:
    SandboxAppLayer(const WindowLayers *layers);

    void OnTransfer(const DeltaTime &deltaTime) override;

    template <Dimension D> void DrawShapes();
    template <Dimension D> Shape<D> CreateShape(const ContextData<D> &context);
    template <Dimension D> RenderContext<D> *AddContext();
    template <Dimension D> auto &GetContexts()
    {
        if constexpr (D == D2)
            return Contexts2;
        else
            return Contexts3;
    }
    template <Dimension D> auto &GetMeshes()
    {
        if constexpr (D == D2)
            return Meshes2;
        else
            return Meshes3;
    }

    template <Dimension D> void AddStaticMesh(const char *name, const StatMeshData<D> &data);
    template <Dimension D> void AddMeshes();

    Contexts<D2> Contexts2{};
    Contexts<D3> Contexts3{};

    Meshes<D2> Meshes2{};
    Meshes<D3> Meshes3{};
};

class SandboxWinLayer final : public WindowLayer
{
  public:
    SandboxWinLayer(ApplicationLayer *appLayer, Window *window, Dimension dim);

    void OnRender(const DeltaTime &deltaTime) override;
    void OnEvent(const Event &event) override;

#ifdef ONYX_ENABLE_IMGUI
    void RenderImGui();
    template <Dimension D> void RenderCameras();
    template <Dimension D> void RenderCamera(CameraData<D> &camera);
    template <Dimension D> void RenderContexts();
    template <Dimension D> void RenderContext(ContextData<D> &context);
    template <Dimension D> void RenderShapePicker(ContextData<D> &context);
    void RenderLightPicker();
    template <Dimension D> void RenderMeshLoad();
#endif

    template <Dimension D> void ProcessEvent(const Event &event);
    template <Dimension D> void AddCamera();

    template <Dimension D> auto &GetCameras()
    {
        if constexpr (D == D2)
            return Cameras2;
        else
            return Cameras3;
    }

    Cameras<D2> Cameras2{};
    Cameras<D3> Cameras3{};

#ifdef ONYX_ENABLE_IMGUI
    bool ImGuiDemoWindow = false;
#    ifdef ONYX_ENABLE_IMPLOT
    bool ImPlotDemoWindow = false;
#    endif
#endif
};

} // namespace Onyx
