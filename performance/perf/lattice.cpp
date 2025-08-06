#include "perf/lattice.hpp"

namespace Onyx::Perf
{
template <Dimension D> void Lattice<D>::Render(RenderContext<D> *p_Context) const noexcept
{
    p_Context->Fill(Color);
    // p_Context->Outline(Onyx::Color::ORANGE);
    // p_Context->OutlineWidth(0.1f);
    if constexpr (D == D2)
        switch (Shape)
        {
        case Shapes2::Triangle:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Triangle(p_Transform, p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::Square:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Square(p_Transform, p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::NGon:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->NGon(p_Transform, p_Lattice.NGonSides, p_Lattice.ShapeSize);
            });
            return;
        case Shapes2::Polygon:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Polygon(p_Transform, p_Lattice.Vertices);
            });
            return;
        case Shapes2::Circle:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Circle(p_Transform, p_Lattice.Diameter, p_Lattice.CircleOptions);
            });
            return;
        case Shapes2::Stadium:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Stadium(p_Transform, p_Lattice.Length, p_Lattice.Diameter);
            });
            return;
        case Shapes2::RoundedSquare:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->RoundedSquare(p_Transform, p_Lattice.ShapeSize, p_Lattice.Diameter);
            });
            return;
        case Shapes2::Mesh:
            Run([p_Context](const Lattice<D2> &p_Lattice, const fmat3 &p_Transform) {
                p_Context->Mesh(p_Transform, p_Lattice.Mesh, p_Lattice.ShapeSize);
            });
            return;
        }
    else
    {
        switch (Shape)
        {
        case Shapes3::Triangle:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Triangle(p_Transform, fvec2{p_Lattice.ShapeSize});
            });
            return;
        case Shapes3::Square:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Square(p_Transform, fvec2{p_Lattice.ShapeSize});
            });
            return;
        case Shapes3::NGon:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->NGon(p_Transform, p_Lattice.NGonSides, p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Polygon:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Polygon(p_Transform, p_Lattice.Vertices);
            });
            return;
        case Shapes3::Circle:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Circle(p_Transform, p_Lattice.Diameter, p_Lattice.CircleOptions);
            });
            return;
        case Shapes3::Stadium:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Stadium(p_Transform, p_Lattice.Length, p_Lattice.Diameter);
            });
            return;
        case Shapes3::RoundedSquare:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->RoundedSquare(p_Transform, fvec2{p_Lattice.ShapeSize}, p_Lattice.Diameter);
            });
            return;
        case Shapes3::Mesh:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Mesh(p_Transform, p_Lattice.Mesh, p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Cube:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Cube(p_Transform, p_Lattice.ShapeSize);
            });
            return;
        case Shapes3::Cylinder:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Cylinder(p_Transform, p_Lattice.Length, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::Sphere:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Sphere(p_Transform, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::Capsule:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->Capsule(p_Transform, p_Lattice.Length, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        case Shapes3::RoundedCube:
            Run([p_Context](const Lattice<D3> &p_Lattice, const fmat4 &p_Transform) {
                p_Context->RoundedCube(p_Transform, p_Lattice.ShapeSize, p_Lattice.Diameter, p_Lattice.Res);
            });
            return;
        }
    }
}
template struct Lattice<D2>;
template struct Lattice<D3>;
} // namespace Onyx::Perf
