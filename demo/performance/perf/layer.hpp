#pragma once

#include "perf/lattice.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"

namespace Onyx::Demo
{
template <Dimension D> class Layer : public UserLayer
{
  public:
    Layer(SingleWindowApp *p_Application, TKit::Span<const Lattice<D>> p_Lattices);

    void OnUpdate() override;
    void OnEvent(const Event &p_Event) override;

  private:
    SingleWindowApp *m_Application;
    Window *m_Window;
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    TKit::StaticArray8<Lattice<D>> m_Lattices{};
};
} // namespace Onyx::Demo
