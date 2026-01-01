#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/user_layer.hpp"

#ifndef ONYX_SANDBOX_MAX_SHAPES
#    define ONYX_SANDBOX_MAX_SHAPES 64
#endif
#ifndef ONYX_SANDBOX_MAX_LIGHTS
#    define ONYX_SANDBOX_MAX_LIGHTS 64
#endif

namespace Onyx::Demo
{
struct MeshId
{
    std::string Name;
    Mesh Mesh;
};
struct MeshContainer
{
    TKit::Array<MeshId, ONYX_SANDBOX_MAX_SHAPES> StaticMeshes;
    u32 StaticOffset;
};

// order matters here. meshes must go first
enum ShapeType : u8
{
    Shape_Triangle = 0,
    Shape_Square,
    Shape_Cube,
    Shape_Sphere,
    Shape_Cylinder,
    Shape_ImportedStatic,
    Shape_Circle,
};
template <Dimension D> struct Shape
{
    ShapeType Type;
    std::string Name;
    Mesh Mesh = NullMesh;
    Transform<D> Transform{};
    CircleOptions CircleOptions{};
    MaterialData<D> Material{};
    bool Fill = true;
    bool Outline = false;
    f32 OutlineWidth = 0.01f;
    Color OutlineColor = Color::ORANGE;
};

template <Dimension D> struct LatticeData
{
    u32v<D> Dimensions{2};
    f32 Separation = 1.f;
    Shape<D> Shape;
    u32 Partitions = 1;
    bool Enabled = false;
    bool PropToScale = true;
    bool NeedsUpdate = false;
};

template <Dimension D> struct LineTest
{
    f32v<D> Start{0.f};
    f32v<D> End{1.f};
    MaterialData<D> Material{};
    f32 Thickness = 0.05f;
    f32 OutlineWidth = 0.01f;
    Color OutlineColor = Color::ORANGE;
    bool Outline = false;
};

template <Dimension D> struct CameraData;
template <> struct CameraData<D2>
{
    Camera<D2> *Camera;
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

template <Dimension D> struct CameraDataContainer
{
    TKit::Array<CameraData<D>, MaxCameras> Cameras;
    u32 Active = 0;
};

template <Dimension D> struct IContextData
{
    RenderContext<D> *Context;
    TKit::Array<Shape<D>, ONYX_SANDBOX_MAX_SHAPES> Shapes;
    MaterialData<D> AxesMaterial{};

    u32 ShapeToSpawn = Shape_Triangle;
    u32 ImportedStatToSpawn = 0;
    f32 AxesThickness = 0.01f;
    u32 SelectedShape = 0;

    LatticeData<D> Lattice;
    LineTest<D> Line;

    bool DrawAxes = false;
};

template <Dimension D> struct ContextData : IContextData<D>
{
};

template <> struct ONYX_API ContextData<D3> : IContextData<D3>
{
    TKit::Array<DirectionalLight, ONYX_SANDBOX_MAX_LIGHTS> DirectionalLights;
    TKit::Array<PointLight, ONYX_SANDBOX_MAX_LIGHTS> PointLights;
    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};

    bool DrawLights = false;
    int LightToSpawn = 0;
    u32 SelectedDirLight = 0;
    u32 SelectedPointLight = 0;
};

struct BlurData
{
    u32 KernelSize = 1;
    f32 Width = 1.f;
    f32 Height = 1.f;
};

template <Dimension D> struct ContextDataContainer
{
    TKit::Array<ContextData<D>, MaxRenderContexts> Contexts;
    u32 Active = 0;
    bool EmptyContext = false;
};

class SandboxLayer final : public UserLayer
{
  public:
    SandboxLayer(Application *p_Application, Window *p_Window, Dimension p_Dim);

    void OnFrameBegin(const DeltaTime &p_DeltaTime, const FrameInfo &) override;
    void OnRenderBegin(const DeltaTime &, const FrameInfo &p_Info) override;
    void OnEvent(const Event &p_Event) override;

  private:
    void renderImGui();

    template <Dimension D> void drawShapes(const ContextData<D> &p_Context);

#ifdef ONYX_ENABLE_IMGUI

    template <Dimension D> void renderUI(ContextDataContainer<D> &p_Contexts);
    template <Dimension D> void renderUI(ContextData<D> &p_Context);

    template <Dimension D> void renderCameras(CameraDataContainer<D> &p_Container);
    template <Dimension D> void renderCamera(CameraData<D> &p_Camera);

    void renderLightSpawn(ContextData<D3> &p_Context);
#endif

    template <Dimension D> ContextData<D> &addContext(ContextDataContainer<D> &p_Contexts);
    template <Dimension D> void setupContext(ContextData<D> &p_Context);

    template <Dimension D> CameraData<D> &addCamera(CameraDataContainer<D> &p_Cameras);
    void setupCamera(CameraData<D3> &p_Camera);

    ContextDataContainer<D2> m_ContextData2{};
    ContextDataContainer<D3> m_ContextData3{};

    CameraDataContainer<D2> m_Cameras2{};
    CameraDataContainer<D3> m_Cameras3{};

    MeshContainer m_Meshes2;
    MeshContainer m_Meshes3;

    BlurData m_BlurData{};
    VKit::GraphicsJob m_RainbowJob{};

    VKit::PipelineLayout m_BlurLayout{};

    bool m_RainbowBackground = false;
    bool m_PostProcessing = false;
};
} // namespace Onyx::Demo
