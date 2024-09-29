#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/draw/primitives/primitives.hpp"

namespace ONYX
{
class ExampleLayer final : public Layer
{
  public:
    ExampleLayer() noexcept;

    void OnRender(usize p_WindowIndex) noexcept override;
    void OnImGuiRender() noexcept override;
    bool OnEvent(usize p_WindowIndex, const Event &p_Event) noexcept override;

  private:
    void renderWindowSpawner() noexcept;
    void renderWindowController() noexcept;
    ONYX_DIMENSION_TEMPLATE void renderObjectProperties(usize p_WindowIndex) noexcept;

    // This is awful. But its also a demo
    DynamicArray<DynamicArray<KIT::Scope<ONYX::IDrawable>>> m_DrawObjects;
};
} // namespace ONYX