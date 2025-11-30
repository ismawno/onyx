#pragma once

#include "sbox/shapes.hpp"
#include "sbox/argparse.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/app/app.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/profiling/timespan.hpp"
#include "tkit/memory/ptr.hpp"

namespace Onyx::Demo
{
template <Dimension D> struct LatticeData
{
    u32v<D> Dimensions{2};
    f32 Separation = 1.f;
    TKit::Scope<Shape<D>> Shape;
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
    bool Rounded = false;
    bool Outline = false;
};

template <Dimension D> struct ICameraData
{
    Camera<D> *Camera;
};

template <Dimension D> struct CameraData : ICameraData<D>
{
};

template <> struct ONYX_API CameraData<D3> : ICameraData<D3>
{
    f32 FieldOfView = Math::Radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
    f32 ZOffset = 0.f;

    bool Perspective = false;
};

template <Dimension D> struct CameraDataContainer
{
    TKit::StaticArray<CameraData<D>, ONYX_MAX_CAMERAS> Cameras;
    u32 Active = 0;
};

template <Dimension D> struct IContextData
{
    RenderContext<D> *Context;
    TKit::DynamicArray<TKit::Scope<Shape<D>>> Shapes;
    MaterialData<D> AxesMaterial{};

    PolygonVerticesArray PolygonVertices;
    NamedMesh<D> Mesh{};
    i32 ShapeToSpawn = 0;
    i32 MeshToSpawn = 0;
    i32 NGonSides = 3;
    f32 AxesThickness = 0.01f;
    u32 SelectedShape = 0;
    f32v2 VertexToAdd{0.f};

    LatticeData<D> Lattice;
    LineTest<D> Line;

    bool DrawAxes = false;
};

template <Dimension D> struct ContextData : IContextData<D>
{
};

template <> struct ONYX_API ContextData<D3> : IContextData<D3>
{
    TKit::DynamicArray<DirectionalLight> DirectionalLights;
    TKit::DynamicArray<PointLight> PointLights;
    f32v4 Ambient = f32v4{1.f, 1.f, 1.f, 0.4f};

    bool DrawLights = false;
    int LightToSpawn = 0;
    u32 SelectedDirLight = 0;
    u32 SelectedPointLight = 0;
};

struct ONYX_API BlurData
{
    u32 KernelSize = 1;
    f32 Width = 1.f;
    f32 Height = 1.f;
};

template <Dimension D> struct ContextDataContainer
{
    TKit::StaticArray<ContextData<D>, ONYX_MAX_RENDER_CONTEXTS> Contexts;
    u32 Active = 0;
    bool EmptyContext = false;
};

struct QuitResult
{
    ApplicationType Type;
    bool Reload;
};

class ONYX_API WindowData
{
  public:
    WindowData() = default;
    WindowData(Window *p_Window, Dimension p_Dim);

    void OnUpdate(TKit::Timespan p_Timestep);
    void OnRenderBegin(VkCommandBuffer p_CommandBuffer);
    void OnImGuiRender();
    void OnEvent(const Event &p_Event);

    static QuitResult OnImGuiRenderGlobal(IApplication *p_Application, TKit::Timespan p_Timestep,
                                          ApplicationType p_CurrentType);
    static void RenderEditorText();

  private:
    template <Dimension D> void drawShapes(const ContextData<D> &p_Context);

    template <Dimension D> void renderUI(ContextDataContainer<D> &p_Contexts);
    template <Dimension D> void renderUI(ContextData<D> &p_Context);

    template <Dimension D> void renderCameras(CameraDataContainer<D> &p_Container);
    template <Dimension D> void renderCamera(CameraData<D> &p_Camera);

    void renderLightSpawn(ContextData<D3> &p_Context);

    template <Dimension D> ContextData<D> &addContext(ContextDataContainer<D> &p_Contexts);
    template <Dimension D> void setupContext(ContextData<D> &p_Context);

    template <Dimension D> CameraData<D> &addCamera(CameraDataContainer<D> &p_Cameras);
    void setupCamera(CameraData<D3> &p_Camera);

    Window *m_Window = nullptr;
    ContextDataContainer<D2> m_ContextData2{};
    ContextDataContainer<D3> m_ContextData3{};

    CameraDataContainer<D2> m_Cameras2{};
    CameraDataContainer<D3> m_Cameras3{};

    Color m_BackgroundColor = Color::BLACK;

    BlurData m_BlurData{};
    VKit::GraphicsJob m_RainbowJob{};

    VKit::PipelineLayout m_BlurLayout{};

    bool m_RainbowBackground = false;
    bool m_PostProcessing = false;
};

} // namespace Onyx::Demo
