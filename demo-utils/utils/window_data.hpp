#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "utils/shapes.hpp"
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

template <Dimension D> struct ILayerData
{
    RenderContext<D> *Context;
    TKit::DynamicArray<TKit::Scope<Shape<D>>> Shapes;
    Transform<D> AxesTransform{};
    MaterialData<D> AxesMaterial{};

    TKit::StaticArray<fvec2, ONYX_MAX_POLYGON_VERTICES> PolygonVertices;
    i32 ShapeToSpawn = 0;
    f32 AxesThickness = 0.01f;
    LatticeData<D> Lattice;

    bool DrawAxes = false;
};

template <Dimension D> struct LayerData : ILayerData<D>
{
};

template <> struct LayerData<D3> : ILayerData<D3>
{
    TKit::DynamicArray<DirectionalLight> DirectionalLights;
    TKit::DynamicArray<PointLight> PointLights;
    fvec4 Ambient = fvec4{1.f, 1.f, 1.f, 0.4f};

    f32 FieldOfView = glm::radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
    f32 ZOffset = 0.f;

    bool Perspective = false;
    bool DrawLights = false;

    int LightToSpawn = 0;
    DirectionalLight DirLightToAdd{fvec4{1.f, 1.f, 1.f, 1.f}, Color::WHITE};
    PointLight PointLightToAdd{fvec4{0.f, 0.f, 0.f, 1.f}, Color::WHITE, 1.f};
    u32 SelectedDirLight = 0;
    u32 SelectedPointLight = 0;
};

struct BlurData
{
    u32 KernelSize = 1;
    f32 Width = 1.f;
    f32 Height = 1.f;
};

class WindowData
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
    template <Dimension D> void renderUI(LayerData<D> &p_Data) noexcept;
    void renderLightSpawn() noexcept;

    Window *m_Window = nullptr;
    LayerData<D2> m_LayerData2{};
    LayerData<D3> m_LayerData3{};
    Color m_BackgroundColor = Color::BLACK;

    BlurData m_BlurData{};
    VKit::GraphicsJob m_RainbowJob{};

    VKit::PipelineLayout m_BlurLayout{};

    bool m_RainbowBackground = false;
    bool m_PostProcessing = false;
};

} // namespace Onyx::Demo