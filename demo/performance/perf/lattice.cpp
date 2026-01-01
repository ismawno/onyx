#include "perf/lattice.hpp"

namespace Onyx::Demo
{
template <Dimension D> void Lattice<D>::StaticMesh(RenderContext<D> *p_Context, const Mesh p_Mesh) const
{
    p_Context->Fill(Color);
    p_Context->Transform(Transform.ComputeTransform());
    p_Context->ShareCurrentState();

    Run([=](const f32v<D> &p_Pos) {
        p_Context->SetTranslation(p_Pos);
        p_Context->StaticMesh(p_Mesh);
    });
}
template <Dimension D> void Lattice<D>::Circle(RenderContext<D> *p_Context, const CircleOptions &p_Options) const
{
    p_Context->Fill(Color);
    p_Context->Transform(Transform.ComputeTransform());
    p_Context->ShareCurrentState();

    Run([p_Context, &p_Options](const f32v<D> &p_Pos) {
        p_Context->SetTranslation(p_Pos);
        p_Context->Circle(p_Options);
    });
}
template struct Lattice<D2>;
template struct Lattice<D3>;
} // namespace Onyx::Demo
