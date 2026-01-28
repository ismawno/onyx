#pragma once

#include "perf/lattice.hpp"
#include "onyx/application/app.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/context.hpp"

namespace Onyx::Demo
{
template <Dimension D> class Layer : public UserLayer
{
  public:
    Layer(Application *application, Window *window, const Lattice<D> &lattice, const ShapeSettings &options);

    void OnFrameBegin(const DeltaTime &deltaTime, const FrameInfo &) override;
    void OnEvent(const Event &event) override;

  private:
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    Lattice<D> m_Lattice;
    ShapeSettings m_Options;
    Mesh m_Mesh;
    Mesh m_AxesMesh;
};
} // namespace Onyx::Demo
