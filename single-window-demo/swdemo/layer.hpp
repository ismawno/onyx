#pragma once

#include "onyx/app/app.hpp"
#include "onyx/app/layer.hpp"

#include "swdemo/shapes.hpp"

namespace ONYX
{
template <Dimension D> struct LayerData
{
    RenderContext<D> *Context;
    DynamicArray<KIT::Scope<Shape<D>>> Shapes;
    mat<D> AxesTransform{1.f};
    MaterialData<D> AxesMaterial{};

    DynamicArray<vec<D>> PolygonVertices;
    i32 ShapeToSpawn = 0;
    f32 AxesThickness = 0.01f;
    bool DrawAxes = false;
    bool ControlAsCamera = true;
};

class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender() noexcept override;
    bool OnEvent(const Event &p_Event) noexcept override;

  private:
    template <Dimension D> void drawShapes(const LayerData<D> &p_Data) noexcept;
    template <Dimension D> void renderUI(LayerData<D> &p_Data) noexcept;
    template <Dimension D> void controlAxes(LayerData<D> &p_Data) noexcept;
    void renderLightSpawn() noexcept;

    Application *m_Application = nullptr;

    LayerData<D2> m_LayerData2;
    LayerData<D3> m_LayerData3;

    Color m_BackgroundColor = Color::BLACK;

    bool m_Perspective = false;
    f32 m_FieldOfView = glm::radians(75.f);
    f32 m_Near = 0.1f;
    f32 m_Far = 100.f;
    f32 m_ZOffset = 0.f;

    DynamicArray<DirectionalLight> m_DirectionalLights;
    DynamicArray<PointLight> m_PointLights;
    vec4 m_Ambient = vec4{1.f, 1.f, 1.f, 0.4f};
    bool m_DrawLights = false;
};
} // namespace ONYX