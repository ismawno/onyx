#pragma once

#include "onyx/app/app.hpp"
#include "onyx/app/layer.hpp"

#include "swdemo/shapes.hpp"

namespace ONYX
{
class SWExampleLayer final : public Layer
{
  public:
    SWExampleLayer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender() noexcept override;
    void OnImGuiRender() noexcept override;

  private:
    ONYX_DIMENSION_TEMPLATE void drawShapes(RenderContext<N> *p_Context,
                                            const DynamicArray<KIT::Scope<Shape<N>>> &p_Shapes,
                                            const Transform<N> &p_Axes) noexcept;

    ONYX_DIMENSION_TEMPLATE void renderShapeSpawn(DynamicArray<KIT::Scope<Shape<N>>> &p_Shapes) noexcept;

    Application *m_Application = nullptr;

    RenderContext2D *m_Context2;
    RenderContext3D *m_Context3;

    DynamicArray<KIT::Scope<Shape2D>> m_Shapes2;
    DynamicArray<KIT::Scope<Shape3D>> m_Shapes3;

    Transform2D m_Axes2;
    Transform3D m_Axes3;

    Color m_BackgroundColor = Color::BLACK;
    Color m_ShapeColor = Color::WHITE;

    usize m_Selected2;
    usize m_Selected3;

    bool m_Perspective = false;
    f32 m_FieldOfView = glm::radians(75.f);
    f32 m_Near = 0.1f;
    f32 m_Far = 100.f;
};
} // namespace ONYX