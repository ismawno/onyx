#include "perf/lattice.hpp"

namespace Onyx::Perf
{
template <Dimension D> static fmat<D> setPos(fmat<D> p_Transform, const fvec<D> &p_Pos) noexcept
{
    Transform<D>::TranslateExtrinsic(p_Transform, p_Pos);
    return p_Transform;
}
template <Dimension D> void Lattice<D>::Render(RenderContext<D> *p_Context) const noexcept
{
    const fvec<D> offset = Transform.Translation;
    const fmat<D> transform = Transform.ComputeTransform();
    if constexpr (D == D2)
        switch (Shape)
        {
        case Shapes2::Triangle:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Triangle(t, ShapeSize);
            });
            return;
        case Shapes2::Square:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Square(t, ShapeSize);
            });
            return;
        case Shapes2::NGon:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->NGon(t, NGonSides);
            });
            return;
        case Shapes2::Polygon:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Polygon(t, Vertices);
            });
            return;
        case Shapes2::Circle:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Circle(t, Diameter, Options);
            });
            return;
        case Shapes2::Stadium:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Stadium(t, Length, Diameter);
            });
            return;
        case Shapes2::RoundedSquare:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->RoundedSquare(t, ShapeSize, Diameter);
            });
            return;
        case Shapes2::Mesh:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) {
                const auto t = setPos<D2>(transform, p_Pos + offset);
                p_Context->Mesh(t, Mesh, ShapeSize);
            });
            return;
        }
    else
    {
        const fvec2 shapeSize2 = fvec2{ShapeSize};
        switch (Shape)
        {
        case Shapes3::Triangle:
            Run([p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Triangle(t, shapeSize2);
            });
            return;
        case Shapes3::Square:
            Run([p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Square(t, shapeSize2);
            });
            return;
        case Shapes3::NGon:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->NGon(t, NGonSides);
            });
            return;
        case Shapes3::Polygon:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Polygon(t, Vertices);
            });
            return;
        case Shapes3::Circle:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Circle(t, Diameter, Options);
            });
            return;
        case Shapes3::Stadium:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Stadium(t, Length, Diameter);
            });
            return;
        case Shapes3::RoundedSquare:
            Run([this, p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->RoundedSquare(t, shapeSize2, Diameter);
            });
            return;
        case Shapes3::Mesh:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Mesh(t, Mesh, ShapeSize);
            });
            return;
        case Shapes3::Cube:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Cube(t, ShapeSize);
            });
            return;
        case Shapes3::Cylinder:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Cylinder(t, Length, Diameter, Res);
            });
            return;
        case Shapes3::Sphere:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Sphere(t, Diameter, Res);
            });
            return;
        case Shapes3::Capsule:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->Capsule(t, Length, Diameter, Res);
            });
            return;
        case Shapes3::RoundedCube:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) {
                const auto t = setPos<D3>(transform, p_Pos + offset);
                p_Context->RoundedCube(t, ShapeSize, Diameter, Res);
            });
            return;
        }
    }
}
template struct Lattice<D2>;
template struct Lattice<D3>;
} // namespace Onyx::Perf
