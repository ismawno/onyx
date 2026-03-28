#pragma once

#include "onyx/application/layer.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/platform/dialog.hpp"
#include "onyx/asset/sampler.hpp"
#include "onyx/asset/texture.hpp"
#include "onyx/asset/mesh.hpp"

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

template <typename Vertex> struct MeshId
{
    std::string Name{};
    Asset Handle = NullHandle;
    MeshData<Vertex> Data{};
};

template <typename Vertex> struct MeshArray
{
    TKit::TierArray<MeshId<Vertex>> Elements{};
    u32 Active = 0;
};

template <typename Vertex> struct MeshPoolId
{
    std::string Name{};
    AssetPool Handle = NullHandle;
    MeshArray<Vertex> Data{};
};

template <Dimension D> using StatMeshId = MeshId<StatVertex<D>>;
template <Dimension D> using StatMeshPoolId = MeshPoolId<StatVertex<D>>;

template <Dimension D> using ParaMeshId = MeshId<ParaVertex<D>>;
template <Dimension D> using ParaMeshPoolId = MeshPoolId<ParaVertex<D>>;

template <Dimension D> struct MeshPoolArray
{
    TKit::TierArray<StatMeshPoolId<D>> StatPools{};
    TKit::TierArray<ParaMeshPoolId<D>> ParaPools{};
    u32 Active = 0;
    u32 GeometryToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};

    Asset DefaultAxesMesh = NullHandle;
};

template <> struct MeshPoolArray<D3>
{
    TKit::TierArray<StatMeshPoolId<D3>> StatPools{};
    TKit::TierArray<ParaMeshPoolId<D3>> ParaPools{};
    u32 Active = 0;
    u32 GeometryToLoad = 0;
    u32 StatMeshToLoad = 0;

    u32 RegularPolySides = 3;
    TKit::TierArray<f32v2> PolyVertices{{f32v2{-1.f, -0.5f}, f32v2{1.f, -0.5f}, f32v2{0.f, 1.f}}};
    f32v2 VertexToAdd{0.f};

    Asset DefaultAxesMesh = NullHandle;
    Asset DefaultLightMesh = NullHandle;

    u32 Rings = 32;
    u32 Sectors = 64;
    u32 CylinderSides = 64;
};

TKIT_YAML_SERIALIZE_DECLARE_ENUM(StaticMeshType)
enum StaticMeshType : u8
{
    StaticMesh_Triangle,
    StaticMesh_Quad,
    StaticMesh_Box,
    StaticMesh_Sphere,
    StaticMesh_Cylinder,
    StaticMesh_Count,
};

template <Dimension D> struct Shape
{
    Geometry Geo = Geometry_Count;
    Asset Mesh = NullHandle;
    std::string Name;
    Asset Material = NullHandle;
    Transform<D> Transform{};
    CircleParameters CircleParameters{};
    InstanceParameters Parameters{};
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
    Geometry GeometryToSpawn = Geometry_Circle;
    TKit::FixedArray<Asset, Geometry_Count> MeshToSpawn{NullHandle, NullHandle, NullHandle};
    f32 AxesThickness = 0.01f;

    Asset AxesMesh = NullHandle;
    Asset AxesMaterial = NullHandle;
    Asset LightMaterial = NullHandle;

    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};
    u32 SelectedShape = 0;
    u32 SelectedPointLight = 0;
    u32 LightToSpawn = 0;

    SandboxFlags Flags = SandboxFlag_DrawLights | SandboxFlag_ContextShouldUpdate;
};

template <> struct ContextData<D3>
{
    RenderContext<D3> *Context;
    TKit::TierArray<Shape<D3>> Shapes;
    TKit::TierArray<PointLight<D3> *> PointLights{};
    TKit::TierArray<DirectionalLight *> DirLights{};
    Geometry GeometryToSpawn = Geometry_Circle;
    TKit::FixedArray<Asset, Geometry_Count> MeshToSpawn{NullHandle, NullHandle, NullHandle};
    f32 AxesThickness = 0.01f;

    u32 SelectedShape = 0;

    Asset AxesMesh = NullHandle;
    Asset AxesMaterial = NullHandle;
    Asset LightMaterial = NullHandle;
    Asset LightMesh = NullHandle;

    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};
    u32 LightToSpawn = 0;
    u32 SelectedPointLight = 0;
    u32 SelectedDirLight = 0;

    SandboxFlags Flags = SandboxFlag_DrawLights | SandboxFlag_ContextShouldUpdate;
};

template <Dimension D> struct ContextArray
{
    TKit::TierArray<ContextData<D>> Contexts{};
    u32 Active = 0;
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
    Geometry Geo = Geometry_Circle;
    TKIT_YAML_SERIALIZE_GROUP_BEGIN("StatMesh", "--deserialize-as StaticMeshType")
    Asset StatMesh = 0;
    TKIT_YAML_SERIALIZE_GROUP_END()
    TKIT_YAML_SERIALIZE_GROUP_BEGIN("ParaMesh", "--deserialize-as ParametricShape")
    Asset ParaMesh = 0;
    TKIT_YAML_SERIALIZE_GROUP_END()
    u32 Threads = 1;
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    SandboxFlags Flags = SandboxFlag_ContextShouldUpdate;
    TKIT_YAML_SERIALIZE_IGNORE_END()
};

template <Dimension D> struct MaterialId
{
    std::string Name{};
    Asset Handle = NullHandle;
    MaterialData<D> Data{};
};

template <Dimension D> struct MaterialArray
{
    TKit::TierArray<MaterialId<D>> Elements{};
    u32 Active = 0;
};

template <Dimension D> struct MaterialPoolId
{
    std::string Name{};
    AssetPool Handle = NullHandle;
    MaterialArray<D> Data{};
};

template <Dimension D> struct MaterialPoolArray
{
    TKit::TierArray<MaterialPoolId<D>> Pools{};
    u32 Active = 0;
    Asset DefaultMaterial = NullHandle;
};

struct SamplerId
{
    std::string Name{};
    Asset Handle = NullHandle;
    SamplerData Data{};
};

struct TextureId
{
    std::string Name{};
    Asset Handle = NullHandle;
};

template <Dimension D> struct LatticeArray
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

    template <Dimension D> Shape<D> CreateShape(Geometry geo, Asset mesh = NullHandle);

    template <Dimension D> void AddContext(const Window *window = nullptr);
    template <Dimension D> void AddLattice(const Window *window = nullptr, const LatticeData<D> &lattice = {});

    template <typename Vertex>
    MeshPoolId<Vertex> &AddMeshPool(TKit::TierArray<MeshPoolId<Vertex>> &pools, const char *name = nullptr);

    template <typename Vertex>
    MeshId<Vertex> &AddMesh(MeshPoolId<Vertex> &pool, const MeshData<Vertex> &data, const char *name = nullptr);

    template <Dimension D> MaterialPoolId<D> &AddMaterialPool(const char *name = nullptr);
    template <Dimension D> MaterialId<D> &AddMaterial(MaterialPoolId<D> &pool, const char *name = nullptr);

    void AddSampler(const char *name = nullptr);
    void AddTexture(const TextureData &data, const char *name = nullptr);

    template <Dimension D> void UpdateMaterialData();

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

    template <Dimension D> void AddDefaultMeshes();
    template <Dimension D> void AddDefaultMaterial();

    ContextArray<D2> Contexts2{};
    ContextArray<D3> Contexts3{};

    MeshPoolArray<D2> Meshes2{};
    MeshPoolArray<D3> Meshes3{};

    LatticeArray<D2> Lattices2{};
    LatticeArray<D3> Lattices3{};

    MaterialPoolArray<D2> Materials2{};
    MaterialPoolArray<D3> Materials3{};

    TKit::TierArray<SamplerId> Samplers{};
    TKit::TierArray<TextureId> Textures{};
    u32 SelectedSampler = 0;
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
    template <Dimension D> void RenderMeshPools();
    template <typename Vertex> void RenderMeshPool(MeshPoolId<Vertex> &pool);
    template <typename Vertex> void RenderMesh(MeshId<Vertex> &mesh);
    template <Dimension D> void RenderMaterialPools();
    template <Dimension D> void RenderMaterialPool(MaterialPoolId<D> &pool);
    template <Dimension D> void RenderMaterial(MaterialId<D> &material);
    void RenderSamplers();
    void RenderSampler(SamplerId &sampler);
    void RenderTextures();
    template <Dimension D> void RenderGltf();
    template <Dimension D> void RenderRenderer();

    template <typename F>
    void HandleLoadDialog(TKit::Task<Dialog::Result<Dialog::Path>> &task, F load, const char *name = "Load##Dialog");
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

    TKit::Task<Dialog::Result<Dialog::Path>> StatMeshTask{};
    TKit::Task<Dialog::Result<Dialog::Path>> TexTask{};
    TKit::Task<Dialog::Result<Dialog::Path>> GltfTask{};

    bool CreatePoolOnLoad = true;
#ifdef ONYX_ENABLE_IMGUI
    bool ImGuiDemoWindow = false;
#    ifdef ONYX_ENABLE_IMPLOT
    bool ImPlotDemoWindow = false;
#    endif
#endif
    u32 TabSelect = 0;
};

} // namespace Onyx
