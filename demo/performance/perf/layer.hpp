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
    Layer(Application *p_Application, Window *p_Window, TKit::Span<const Lattice<D>> p_Lattices);

    void OnFrameBegin(const DeltaTime &p_DeltaTime, const FrameInfo &) override;
    void OnEvent(const Event &p_Event) override;

  private:
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    TKit::StaticArray8<Lattice<D>> m_Lattices{};
};
} // namespace Onyx::Demo
