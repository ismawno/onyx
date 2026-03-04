#pragma once

#include "onyx/application/layer.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/platform/dialog.hpp"

namespace Onyx
{
using SandboxFlags = u32;
enum SandboxFlagBit : SandboxFlags
{
    SandboxFlag_Fill = 1 << 0,
    SandboxFlag_Outline = 1 << 1,
    SandboxFlag_DrawLights = 1 << 2,
    SandboxFlag_DrawAxes = 1 << 3,
    SandboxFlag_ContextShouldUpdate = 1 << 4,
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

template <Dimension D> struct CameraArray
{
    TKit::TierArray<CameraData<D>> Cameras{};
    u32 Active = 0;
};

struct MeshId
{
    std::string Name;
    Mesh Mesh;
};

TKIT_YAML_SERIALIZE_DECLARE_ENUM(StaticMeshType)
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
    Mesh StatMesh = NullMesh;
    Material Material = NullMaterial;
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
    TKit::TierArray<PointLight<D> *> PointLights{};
    u32 GeometryToSpawn = Geometry_Circle;
    u32 StatMeshToSpawn = 0;
    f32 AxesThickness = 0.01f;
    Material AxesMaterial = NullMaterial;

    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};
    u32 SelectedShape = 0;
    u32 SelectedPointLight = 0;
    u32 LightToSpawn = 0;

    SandboxFlags Flags = SandboxFlag_ContextShouldUpdate;
};

template <> struct ContextData<D3>
{
    RenderContext<D3> *Context;
    TKit::TierArray<Shape<D3>> Shapes;
    TKit::TierArray<PointLight<D3> *> PointLights{};
    TKit::TierArray<DirectionalLight *> DirLights{};
    u32 GeometryToSpawn = Geometry_Circle;
    u32 StatMeshToSpawn = 0;
    u32 SelectedShape = 0;
    f32 AxesThickness = 0.01f;
    Material AxesMaterial = NullMaterial;

    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};
    u32 LightToSpawn = 0;
    u32 SelectedPointLight = 0;
    u32 SelectedDirLight = 0;

    SandboxFlags Flags = SandboxFlag_ContextShouldUpdate;
};

template <Dimension D> struct ContextArray
{
    TKit::TierArray<ContextData<D>> Contexts{};
    u32 Active = 0;
};

template <Dimension D> struct MeshArray
{
    TKit::TierArray<MeshId> StaticMeshes{};
    u32 GeometryToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};
};

template <> struct MeshArray<D3>
{
    TKit::TierArray<MeshId> StaticMeshes{};
    u32 GeometryToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};

    u32 Rings = 32;
    u32 Sectors = 64;
    u32 CylinderSides = 64;
};

template <Dimension D> struct LatticeData
{
    TKIT_YAML_SERIALIZE_DECLARE(LatticeData)
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    TKit::FixedArray<RenderContext<D> *, TKit::MaxThreads> Contexts{};
    Shape<D> Shape{};
    TKIT_YAML_SERIALIZE_IGNORE_END()
    f32v<D> Position{0.f};
    u32v<D> Dimensions{4};
    f32 Separation = 1.5f;
    TKIT_YAML_SERIALIZE_GROUP_BEGIN("Geo", "--deserialize-as Geometry")
    u32 Geometry = 0;
    TKIT_YAML_SERIALIZE_GROUP_END()
    TKIT_YAML_SERIALIZE_GROUP_BEGIN("StatMesh", "--deserialize-as StaticMeshType")
    u32 StatMesh = 0;
    TKIT_YAML_SERIALIZE_GROUP_END()
    u32 Threads = 1;
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    SandboxFlags Flags = SandboxFlag_ContextShouldUpdate;
    TKIT_YAML_SERIALIZE_IGNORE_END()
};

template <Dimension D> struct MatData
{
    std::string Name{};
    Material Material = NullMaterial;
    MaterialData<D> Data{};
};

template <Dimension D> struct Materials
{
    TKit::TierArray<MatData<D>> Materials{};
    u32 Active = 0;
};

template <Dimension D> struct Lattices
{
    TKit::TierArray<LatticeData<D>> Lattices{};
    u32 Active = 0;
};

struct ParseData;
class SandboxAppLayer final : public ApplicationLayer
{
  public:
    SandboxAppLayer(const WindowLayers *layers, const ParseData *data);

    void OnTransfer(const DeltaTime &deltaTime) override;

    template <Dimension D> void DrawShapes();
    template <Dimension D> void DrawLattices();
    template <Dimension D, typename F> void DrawLattice(const LatticeData<D> &lattice, F &&fun);

    template <Dimension D> Shape<D> CreateShape(u32 geometry, u32 statMesh);
    template <Dimension D> Shape<D> CreateShape(const ContextData<D> &context)
    {
        return CreateShape<D>(context.GeometryToSpawn, context.StatMeshToSpawn);
    }
    template <Dimension D> Shape<D> CreateShape(const LatticeData<D> &lattice)
    {
        return CreateShape<D>(lattice.Geometry, lattice.StatMesh);
    }

    template <Dimension D> void AddContext(const Window *window = nullptr);
    template <Dimension D> void AddLattice(const Window *window = nullptr, const LatticeData<D> &lattice = {});
    template <Dimension D> void AddMaterial();

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
    template <Dimension D> auto &GetLattices()
    {
        if constexpr (D == D2)
            return Lattices2;
        else
            return Lattices3;
    }
    template <Dimension D> auto &GetMaterials()
    {
        if constexpr (D == D2)
            return Materials2;
        else
            return Materials3;
    }

    template <Dimension D> void AddStaticMesh(const char *name, const StatMeshData<D> &data);
    template <Dimension D> void AddMeshes();

    ContextArray<D2> Contexts2{};
    ContextArray<D3> Contexts3{};

    MeshArray<D2> Meshes2{};
    MeshArray<D3> Meshes3{};

    Lattices<D2> Lattices2{};
    Lattices<D3> Lattices3{};

    Materials<D2> Materials2;
    Materials<D3> Materials3;
};

class SandboxWinLayer final : public WindowLayer
{
  public:
    SandboxWinLayer(ApplicationLayer *appLayer, Window *window, Dimension dim);
    ~SandboxWinLayer();

    void OnRender(const DeltaTime &deltaTime) override;
    void OnEvent(const Event &event) override;

#ifdef ONYX_ENABLE_IMGUI
    void RenderImGui();
    template <Dimension D> void RenderCameras();
    template <Dimension D> void RenderCamera(CameraData<D> &camera);
    template <Dimension D> void RenderContexts();
    template <Dimension D> void RenderContext(ContextData<D> &context);
    template <Dimension D> void RenderShapePicker(ContextData<D> &context);
    template <Dimension D> void RenderLightPicker(ContextData<D> &context);
    template <Dimension D> void RenderLattices();
    template <Dimension D> void RenderLattice(LatticeData<D> &lattice);
    template <Dimension D> void RenderMaterials();
    template <Dimension D> void RenderMaterial(MatData<D> &material);
    template <Dimension D> void RenderRenderer();
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

    CameraArray<D2> Cameras2{};
    CameraArray<D3> Cameras3{};

#ifndef TKIT_OS_APPLE
    TKit::Task<Dialog::Result<Dialog::Path>> DialogTask{};
#endif

#ifdef ONYX_ENABLE_IMGUI
    bool ImGuiDemoWindow = false;
#    ifdef ONYX_ENABLE_IMPLOT
    bool ImPlotDemoWindow = false;
#    endif
#endif
};

} // namespace Onyx
