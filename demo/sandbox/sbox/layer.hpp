#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/application/app.hpp"
#include "onyx/application/user_layer.hpp"

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
    TKit::StaticArray<MeshId, ONYX_SANDBOX_MAX_SHAPES> StaticMeshes;
    u32 StaticOffset;
};

// order matters here. meshes must go first
enum class ShapeType2 : u8
{
    Triangle = 0,
    Square,
    ImportedStatic,
    Circle,
};
enum class ShapeType3 : u8
{
    Triangle = 0,
    Square,
    Cube,
    Sphere,
    Cylinder,
    ImportedStatic,
    Circle,
};

template <Dimension D> struct ShapeSelect;
template <> struct ShapeSelect<D2>
{
    using Type = ShapeType2;
};
template <> struct ShapeSelect<D3>
{
    using Type = ShapeType3;
};
template <Dimension D> using ShapeType = typename ShapeSelect<D>::Type;

template <Dimension D> struct Shape
{
    ShapeType<D> Type;
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
    TKit::StaticArray<CameraData<D>, MaxCameras> Cameras;
    u32 Active = 0;
};

template <Dimension D> struct IContextData
{
    RenderContext<D> *Context;
    TKit::StaticArray<Shape<D>, ONYX_SANDBOX_MAX_SHAPES> Shapes;
    MaterialData<D> AxesMaterial{};

    u32 ShapeToSpawn = u32(ShapeType<D>::Triangle);
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

template <> struct ContextData<D3> : IContextData<D3>
{
    TKit::StaticArray<DirectionalLight, ONYX_SANDBOX_MAX_LIGHTS> DirectionalLights;
    TKit::StaticArray<PointLight, ONYX_SANDBOX_MAX_LIGHTS> PointLights;
    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};

    bool DrawLights = false;
    int LightToSpawn = 0;
    u32 SelectedDirLight = 0;
    u32 SelectedPointLight = 0;
};

template <Dimension D> struct ContextDataContainer
{
    TKit::StaticArray<ContextData<D>, MaxRenderContexts> Contexts;
    u32 Active = 0;
    bool EmptyContext = false;
};

class SandboxLayer final : public UserLayer
{
  public:
    SandboxLayer(Application *application, Window *window, Dimension dim);

    void OnFrameBegin(const DeltaTime &deltaTime, const FrameInfo &) override;
    void OnEvent(const Event &event) override;

  private:
    void renderImGui();

    template <Dimension D> void drawShapes(const ContextData<D> &context);

#ifdef ONYX_ENABLE_IMGUI

    template <Dimension D> void renderUI(ContextDataContainer<D> &contexts);
    template <Dimension D> void renderUI(ContextData<D> &context);

    template <Dimension D> void renderCameras(CameraDataContainer<D> &container);
    template <Dimension D> void renderCamera(CameraData<D> &camera);

    void renderLightSpawn(ContextData<D3> &context);
#endif

    template <Dimension D> ContextData<D> &addContext(ContextDataContainer<D> &contexts);
    template <Dimension D> void setupContext(ContextData<D> &context);

    template <Dimension D> CameraData<D> &addCamera(CameraDataContainer<D> &cameras);
    void setupCamera(CameraData<D3> &camera);

    ContextDataContainer<D2> m_ContextData2{};
    ContextDataContainer<D3> m_ContextData3{};

    CameraDataContainer<D2> m_Cameras2{};
    CameraDataContainer<D3> m_Cameras3{};

    MeshContainer m_Meshes2;
    MeshContainer m_Meshes3;
};
} // namespace Onyx::Demo
