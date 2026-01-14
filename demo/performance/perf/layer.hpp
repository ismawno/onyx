#pragma once

#include "perf/lattice.hpp"
#include "onyx/app/app.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/context.hpp"

namespace Onyx::Demo
{
template <Dimension D> class Layer : public UserLayer
{
  public:
    Layer(Application *p_Application, Window *p_Window, const Lattice<D> &p_Lattice, const ShapeSettings &p_Options);

    void OnFrameBegin(const DeltaTime &p_DeltaTime, const FrameInfo &) override;
    void OnEvent(const Event &p_Event) override;

  private:
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    Lattice<D> m_Lattice;
    ShapeSettings m_Options;
    Mesh m_Mesh;
    Mesh m_AxesMesh;
};
} // namespace Onyx::Demo
