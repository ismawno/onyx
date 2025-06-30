#include "perf/lattice.hpp"

namespace Onyx::Perf
{
template <Dimension D> void Lattice<D>::Render(RenderContext<D> *p_Context) noexcept
{
    Onyx::Transform<D> transform = Transform;
    const fvec<D> offset = transform.Position;
    if constexpr (D == D2)
        switch (Shape)
        {
        case Shapes2::Triangle:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Triangle(transform, ShapeSize);
            });
            return;
        case Shapes2::Square:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Square(transform, ShapeSize);
            });
            return;
        case Shapes2::NGon:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->NGon(transform, NGonSides);
            });
            return;
        case Shapes2::Polygon:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Polygon(transform, Vertices);
            });
            return;
        case Shapes2::Circle:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Circle(transform, Diameter, Options);
            });
            return;
        case Shapes2::Stadium:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Stadium(transform, Length, Diameter);
            });
            return;
        case Shapes2::RoundedSquare:
            Run([this, p_Context, &offset, &transform](const fvec2 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->RoundedSquare(transform, Dimensions, Diameter);
            });
            return;
        }
    else
    {
        const fvec2 shapeSize2 = fvec2{ShapeSize};
        switch (Shape)
        {
        case Shapes3::Triangle:
            Run([p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Triangle(transform, shapeSize2);
            });
            return;
        case Shapes3::Square:
            Run([p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Square(transform, shapeSize2);
            });
            return;
        case Shapes3::NGon:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->NGon(transform, NGonSides);
            });
            return;
        case Shapes3::Polygon:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Polygon(transform, Vertices);
            });
            return;
        case Shapes3::Circle:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Circle(transform, Diameter, Options);
            });
            return;
        case Shapes3::Stadium:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Stadium(transform, Length, Diameter);
            });
            return;
        case Shapes3::RoundedSquare:
            Run([this, p_Context, &offset, &transform, &shapeSize2](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->RoundedSquare(transform, shapeSize2, Diameter);
            });
            return;
        case Shapes3::Cube:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Cube(transform, ShapeSize);
            });
            return;
        case Shapes3::Cylinder:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Cylinder(transform, Length, Diameter, Res);
            });
            return;
        case Shapes3::Sphere:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Sphere(transform, Diameter, Res);
            });
            return;
        case Shapes3::Capsule:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->Capsule(transform, Length, Diameter, Res);
            });
            return;
        case Shapes3::RoundedCube:
            Run([this, p_Context, &offset, &transform](const fvec3 &p_Pos) mutable {
                transform.Position = offset + p_Pos;
                p_Context->RoundedCube(transform, ShapeSize, Diameter, Res);
            });
            return;
        }
    }
}
} // namespace Onyx::Perf
