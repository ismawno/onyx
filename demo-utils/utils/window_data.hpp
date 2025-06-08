#pragma once

#include "utils/shapes.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/profiling/timespan.hpp"

namespace Onyx::Demo
{

template <Dimension D> struct LatticeData
{
    uvec<D> Dimensions{2};
    f32 Separation = 1.f;
    TKit::Scope<Shape<D>> Shape;
    bool Enabled = false;
    bool PropToScale = true;
};

template <Dimension D> struct LineTest
{
    fvec<D> Start{0.f};
    fvec<D> End{1.f};
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
    f32 FieldOfView = glm::radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
    f32 ZOffset = 0.f;

    bool Perspective = false;
};

template <Dimension D> struct ILayerData
{
    RenderContext<D> *Context;
    TKit::StaticArray16<CameraData<D>> Cameras;
    TKit::DynamicArray<TKit::Scope<Shape<D>>> Shapes;
    Transform<D> AxesTransform{};
    MaterialData<D> AxesMaterial{};

    TKit::StaticArray<fvec2, ONYX_MAX_POLYGON_VERTICES> PolygonVertices;
    NamedModel<D> Model{};
    i32 ShapeToSpawn = 0;
    i32 NGonSides = 3;
    f32 AxesThickness = 0.01f;
    u32 SelectedShape = 0;
    u32 ActiveCamera = 0;
    fvec2 VertexToAdd{0.f};

    LatticeData<D> Lattice;
    LineTest<D> Line;

    bool DrawAxes = false;
};

template <Dimension D> struct LayerData : ILayerData<D>
{
};

template <> struct ONYX_API LayerData<D3> : ILayerData<D3>
{
    TKit::DynamicArray<DirectionalLight> DirectionalLights;
    TKit::DynamicArray<PointLight> PointLights;
    fvec4 Ambient = fvec4{1.f, 1.f, 1.f, 0.4f};

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

template <Dimension D> struct LayerDataContainer
{
    TKit::StaticArray16<LayerData<D>> Data;
    u32 Selected = 0;
};

class ONYX_API WindowData
{
  public:
    void OnStart(Window *p_Window) noexcept;
    void OnUpdate() noexcept;
    void OnRender(VkCommandBuffer p_CommandBuffer, TKit::Timespan p_Timestep) noexcept;
    void OnImGuiRender() noexcept;
    void OnEvent(const Event &p_Event) noexcept;

    static void OnImGuiRenderGlobal(TKit::Timespan p_Timestep) noexcept;
    static void RenderEditorText() noexcept;

  private:
    template <Dimension D> void drawShapes(const LayerData<D> &p_Data, TKit::Timespan p_Timestep) noexcept;
    template <Dimension D> void renderUI(LayerDataContainer<D> &p_Container) noexcept;
    template <Dimension D> void renderUI(LayerData<D> &p_Data) noexcept;
    template <Dimension D> void renderCamera(CameraData<D> &p_Data) noexcept;

    void renderLightSpawn(LayerData<D3> &p_Data) noexcept;

    Window *m_Window = nullptr;
    LayerDataContainer<D2> m_LayerData2{};
    LayerDataContainer<D3> m_LayerData3{};
    Color m_BackgroundColor = Color::BLACK;

    BlurData m_BlurData{};
    VKit::GraphicsJob m_RainbowJob{};

    VKit::PipelineLayout m_BlurLayout{};

    bool m_RainbowBackground = false;
    bool m_PostProcessing = false;
};

} // namespace Onyx::Demo
