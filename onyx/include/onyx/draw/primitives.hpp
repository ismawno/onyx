#pragma once

#include "onyx/draw/data.hpp"
#include "onyx/buffer/vertex_buffer.hpp" // could actually forward declare this
#include "onyx/buffer/index_buffer.hpp"

#ifndef ONYX_MAX_REGULAR_POLYGON_SIDES
#    define ONYX_MAX_REGULAR_POLYGON_SIDES 8
#endif

#define ONYX_REGULAR_POLYGON_COUNT (ONYX_MAX_REGULAR_POLYGON_SIDES - 2)

// Important: index buffers must always be provided for primitives so that they can be batch rendered nicely

namespace ONYX
{
struct PrimitiveDataLayout
{
    u32 VerticesStart;
    u32 IndicesStart;
    u32 IndicesSize;
};

template <Dimension D> struct IPrimitives
{
    static const VertexBuffer<D> *GetVertexBuffer() noexcept;
    static const IndexBuffer *GetIndexBuffer() noexcept;
    static const PrimitiveDataLayout &GetDataLayout(usize p_PrimitiveIndex) noexcept;

    static KIT_CONSTEVAL usize GetTriangleIndex() noexcept
    {
        return 0;
    }
    static KIT_CONSTEVAL usize GetSquareIndex() noexcept
    {
        return 1;
    }
    static constexpr usize GetNGonIndex(u32 p_Sides) noexcept
    {
        KIT_ASSERT(p_Sides <= ONYX_MAX_REGULAR_POLYGON_SIDES && p_Sides >= 3, "NGon sides must be between 3 and {}",
                   ONYX_MAX_REGULAR_POLYGON_SIDES);
        return (D - 1) * 2 + p_Sides - 3 + (D - 2);
    }
};

template <Dimension D> struct Primitives;

// These are not really meant to be used by user, although there is no problem in doing so, but I wont be providing
// 2D and 3D overloads
template <> struct Primitives<D2> : IPrimitives<D2>
{
    static constexpr usize AMOUNT = 2 + ONYX_REGULAR_POLYGON_COUNT;
};

template <> struct Primitives<D3> : IPrimitives<D3>
{
    static constexpr usize AMOUNT = 5 + ONYX_REGULAR_POLYGON_COUNT;

    static KIT_CONSTEVAL usize GetCubeIndex() noexcept
    {
        return 2;
    }
    static KIT_CONSTEVAL usize GetSphereIndex() noexcept
    {
        return 3;
    }
    static KIT_CONSTEVAL usize GetCylinderIndex() noexcept
    {
        return 4;
    }
};

// Called automatically, user must not call this
void CreateCombinedPrimitiveBuffers() noexcept;
void DestroyCombinedPrimitiveBuffers() noexcept;

} // namespace ONYX