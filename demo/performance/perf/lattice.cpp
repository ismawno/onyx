#include "perf/lattice.hpp"

namespace Onyx::Demo
{
template <Dimension D> void Lattice<D>::StaticMesh(RenderContext<D> *context, const Mesh mesh) const
{
    context->Fill(Color);
    context->Transform(Transform.ComputeTransform());
    context->ShareCurrentState();

    Run([=](const f32v<D> &pos) {
        context->SetTranslation(pos);
        context->StaticMesh(mesh);
    });
}
template <Dimension D> void Lattice<D>::Circle(RenderContext<D> *context, const CircleOptions &options) const
{
    context->Fill(Color);
    context->Transform(Transform.ComputeTransform());
    context->ShareCurrentState();

    Run([context, &options](const f32v<D> &pos) {
        context->SetTranslation(pos);
        context->Circle(options);
    });
}
template struct Lattice<D2>;
template struct Lattice<D3>;
} // namespace Onyx::Demo
