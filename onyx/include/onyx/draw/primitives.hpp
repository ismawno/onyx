#pragma once

#include "onyx/draw/data.hpp"
#include "onyx/rendering/buffer.hpp"

#ifndef ONYX_MAX_REGULAR_POLYGON_SIDES
#    define ONYX_MAX_REGULAR_POLYGON_SIDES 8
#endif

namespace ONYX
{

struct PrimitiveDataLayout
{
    usize VerticesStart;
    usize VerticesSize;
    usize IndicesStart;
    usize IndicesSize;
};

ONYX_DIMENSION_TEMPLATE struct IPrimitives
{
    static const Buffer *GetVertexBuffer() noexcept;
    static const Buffer *GetIndexBuffer() noexcept;
    static const PrimitiveDataLayout &GetDataLayout(usize p_PrimitiveIndex) noexcept;

    static usize GetTriangleIndex() noexcept;
    static usize GetSquareIndex() noexcept;
    static usize GetNGonIndex(u32 p_Sides) noexcept;
};

ONYX_DIMENSION_TEMPLATE struct Primitives;

// These are not really meant to be used by user, although there is no problem in doing so, but I wont be providing
// 2D and 3D overloads
template <> struct Primitives<2> : IPrimitives<2>
{
    static constexpr usize AMOUNT = 2 + ONYX_MAX_REGULAR_POLYGON_SIDES;
};

template <> struct Primitives<3> : Primitives<2>
{
    static constexpr usize AMOUNT = 4 + ONYX_MAX_REGULAR_POLYGON_SIDES;

    static usize GetCubeIndex() noexcept;
    static usize GetSphereIndex() noexcept;
};

using Primitives2D = Primitives<2>;
using Primitives3D = Primitives<3>;

// Called automatically, user must not call this
void CreateCombinedPrimitiveBuffers() noexcept;
void DestroyCombinedPrimitiveBuffers() noexcept;

} // namespace ONYX