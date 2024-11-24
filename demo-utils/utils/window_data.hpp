#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "utils/shapes.hpp"
#include "kit/profiling/timespan.hpp"

namespace ONYX
{
template <Dimension D> struct ILayerData
{
    RenderContext<D> *Context;
    DynamicArray<KIT::Scope<Shape<D>>> Shapes;
    Transform<D> AxesTransform{};
    MaterialData<D> AxesMaterial{};

    DynamicArray<vec<D>> PolygonVertices;
    i32 ShapeToSpawn = 0;
    f32 AxesThickness = 0.01f;
    bool DrawAxes = false;
};

template <Dimension D> struct LayerData : ILayerData<D>
{
};

template <> struct LayerData<D3> : ILayerData<D3>
{
    DynamicArray<DirectionalLight> DirectionalLights;
    DynamicArray<PointLight> PointLights;
    vec4 Ambient = vec4{1.f, 1.f, 1.f, 0.4f};

    f32 FieldOfView = glm::radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
    f32 ZOffset = 0.f;

    bool Perspective = false;
    bool DrawLights = false;

    int LightToSpawn = 0;
    DirectionalLight DirLightToAdd{vec4{1.f, 1.f, 1.f, 1.f}, Color::WHITE};
    PointLight PointLightToAdd{vec4{0.f, 0.f, 0.f, 1.f}, Color::WHITE, 1.f};
    usize SelectedDirLight = 0;
    usize SelectedPointLight = 0;
};

class WindowData
{
  public:
    void OnStart(Window *p_Window) noexcept;
    void OnRender(KIT::Timespan p_Timestep) noexcept;
    void OnImGuiRender() noexcept;
    void OnEvent(const Event &p_Event) noexcept;

    static void OnImGuiRenderGlobal(KIT::Timespan p_Timestep) noexcept;

  private:
    template <Dimension D> void drawShapes(const LayerData<D> &p_Data, KIT::Timespan p_Timestep) noexcept;
    template <Dimension D> void renderUI(LayerData<D> &p_Data) noexcept;
    void renderLightSpawn() noexcept;

    Window *m_Window = nullptr;
    LayerData<D2> m_LayerData2{};
    LayerData<D3> m_LayerData3{};
    Color m_BackgroundColor = Color::BLACK;

    KIT::Scope<std::mutex> m_Mutex = KIT::Scope<std::mutex>::Create();
};

} // namespace ONYX