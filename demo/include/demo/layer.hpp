#pragma once

#include "onyx/app/mwapp.hpp"
#include "onyx/app/layer.hpp"
#include "onyx/draw/primitives/primitives.hpp"

namespace ONYX
{
enum CameraType : int
{
    ORTHOGRAPHIC2D = 0,
    ORTHOGRAPHIC3D,
    PERSPECTIVE3D
};

struct WindowData
{
    WindowData() noexcept = default;
    DynamicArray<KIT::Scope<ONYX::IDrawable>> Drawables;
    CameraType Camera;
};

class ExampleLayer final : public Layer
{
  public:
    ExampleLayer(IMultiWindowApplication *p_Application) noexcept;

    void OnRender(usize p_WindowIndex) noexcept override;
    void OnImGuiRender() noexcept override;
    bool OnEvent(usize p_WindowIndex, const Event &p_Event) noexcept override;

  private:
    void renderWindowSpawner() noexcept;
    void renderWindowController() noexcept;
    ONYX_DIMENSION_TEMPLATE void renderObjectProperties(usize p_WindowIndex) noexcept;

    DynamicArray<WindowData> m_WindowData;
    IMultiWindowApplication *m_Application = nullptr;
};
} // namespace ONYX