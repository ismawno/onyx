#include "perf/lattice.hpp"

namespace Onyx::Perf
{
template <Dimension D> void Lattice<D>::Render(RenderContext<D> *p_Context) const noexcept
{
    p_Context->Fill(Color);
    p_Context->Transform(Transform.ComputeTransform());
    p_Context->ShareCurrentState();
    // p_Context->Outline(Onyx::Color::ORANGE);
    // p_Context->OutlineWidth(0.1f);
    if constexpr (D == D2)
        switch (Shape)
        {
        case Shapes2::Triangle:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Triangle(p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::Square:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Square(p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::NGon:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->NGon(p_Lattice.NGonSides, p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::Polygon:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Polygon(p_Lattice.Vertices);
            });
            return;
        case Shapes2::Circle:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Circle(p_Lattice.Diameter, p_Lattice.CircleOptions);
            });
            return;
        case Shapes2::Stadium:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Stadium(p_Lattice.Length, p_Lattice.Diameter);
            });
            return;
        case Shapes2::RoundedSquare:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->RoundedSquare(p_Lattice.ShapeSize, p_Lattice.Diameter);
            });
            return;
        case Shapes2::Mesh:
            Run([p_Context](const fvec2 &p_Pos, const Lattice<D2> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Mesh(p_Lattice.Mesh, p_Lattice.ShapeSize);
            });
            return;
        }
    else
    {
        switch (Shape)
        {
        case Shapes3::Triangle:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Triangle(fvec2{p_Lattice.ShapeSize});
            });
            return;
        case Shapes3::Square:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Square(fvec2{p_Lattice.ShapeSize});
            });
            return;
        case Shapes3::NGon:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->NGon(p_Lattice.NGonSides, p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Polygon:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Polygon(p_Lattice.Vertices);
            });
            return;
        case Shapes3::Circle:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Circle(p_Lattice.Diameter, p_Lattice.CircleOptions);
            });
            return;
        case Shapes3::Stadium:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Stadium(p_Lattice.Length, p_Lattice.Diameter);
            });
            return;
        case Shapes3::RoundedSquare:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->RoundedSquare(fvec2{p_Lattice.ShapeSize}, p_Lattice.Diameter);
            });
            return;
        case Shapes3::Mesh:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Mesh(p_Lattice.Mesh, p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Cube:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Cube(p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Cylinder:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Cylinder(p_Lattice.Length, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::Sphere:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Sphere(p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::Capsule:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->Capsule(p_Lattice.Length, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::RoundedCube:
            Run([p_Context](const fvec3 &p_Pos, const Lattice<D3> &p_Lattice) {
                p_Context->SetTranslation(p_Pos);
                p_Context->RoundedCube(p_Lattice.ShapeSize, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        }
    }
}
template struct Lattice<D2>;
template struct Lattice<D3>;
} // namespace Onyx::Perf
