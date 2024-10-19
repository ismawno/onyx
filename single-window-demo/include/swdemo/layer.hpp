#pragma once

#include "onyx/app/app.hpp"
#include "onyx/app/layer.hpp"

#include "swdemo/shapes.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct LayerData
{
    RenderContext<N> *Context;
    DynamicArray<KIT::Scope<Shape<N>>> Shapes;
    mat<N> Axes{1.f};
    usize Selected = 0;

    DynamicArray<vec<N>> PolygonVertices;
    i32 ShapeToSpawn = 0;
    i32 NGonSides = 3;
    f32 AxesThickness = 0.01f;
    bool DrawAxes = false;
    bool ControlAxes = false;
    bool ControlAsCamera = true;
};

using LayerData2D = LayerData<2>;
using LayerData3D = LayerData<3>;

class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender() noexcept override;
    bool OnEvent(const Event &p_Event) noexcept override;

  private:
    ONYX_DIMENSION_TEMPLATE void drawShapes(const LayerData<N> &p_Data) noexcept;
    ONYX_DIMENSION_TEMPLATE void renderUI(LayerData<N> &p_Data) noexcept;
    ONYX_DIMENSION_TEMPLATE void controlAxes(LayerData<N> &p_Data) noexcept;

    Application *m_Application = nullptr;

    LayerData2D m_LayerData2;
    LayerData3D m_LayerData3;

    Color m_BackgroundColor = Color::BLACK;
    Color m_ShapeColor = Color::WHITE;

    bool m_Perspective = false;
    f32 m_FieldOfView = glm::radians(75.f);
    f32 m_Near = 0.1f;
    f32 m_Far = 100.f;
    f32 m_ZOffset = 0.f;
};
} // namespace ONYX