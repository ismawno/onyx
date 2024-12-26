#pragma once

#include "onyx/draw/data.hpp"

#ifndef ONYX_MAX_REGULAR_POLYGON_SIDES
#    define ONYX_MAX_REGULAR_POLYGON_SIDES 8
#endif

#define ONYX_REGULAR_POLYGON_COUNT (ONYX_MAX_REGULAR_POLYGON_SIDES - 2)

// Important: index buffers must always be provided for primitives so that they can be batch rendered nicely

namespace Onyx
{
/**
 * @brief Layout information for primitive data.
 *
 * Contains the starting indices and sizes for vertices and indices of a primitive shape.
 */
struct PrimitiveDataLayout
{
    /// The starting index of vertices in the combined vertex buffer.
    u32 VerticesStart;
    /// The starting index of indices in the combined index buffer.
    u32 IndicesStart;
    /// The number of indices for the primitive.
    u32 IndicesSize;
};

/**
 * @brief Interface for accessing primitive shapes in a specific dimension.
 *
 * Provides methods to retrieve vertex and index buffers, as well as data layouts for primitives.
 *
 * @tparam D The dimension (D2 or D3).
 */
template <Dimension D> struct IPrimitives
{
    /**
     * @brief Get the combined vertex buffer for the primitives.
     *
     * @return The vertex buffer.
     */
    static const VertexBuffer<D> &GetVertexBuffer() noexcept;

    /**
     * @brief Get the combined index buffer for the primitives.
     *
     * @return The index buffer.
     */
    static const IndexBuffer &GetIndexBuffer() noexcept;

    /**
     * @brief Get the data layout for a specific primitive.
     *
     * @param p_PrimitiveIndex The index of the primitive.
     * @return The data layout of the primitive.
     */
    static const PrimitiveDataLayout &GetDataLayout(usize p_PrimitiveIndex) noexcept;

    /**
     * @brief Get the index of the triangle primitive.
     *
     * @return Index of the triangle primitive.
     */
    static TKIT_CONSTEVAL usize GetTriangleIndex() noexcept
    {
        return 0;
    }

    /**
     * @brief Get the index of the square primitive.
     *
     * @return Index of the square primitive.
     */
    static TKIT_CONSTEVAL usize GetSquareIndex() noexcept
    {
        return 1;
    }

    /**
     * @brief Get the index of an n-gon primitive with the specified number of sides.
     *
     * @param p_Sides Number of sides of the n-gon.
     * @return Index of the n-gon primitive.
     */
    static constexpr usize GetNGonIndex(u32 p_Sides) noexcept
    {
        TKIT_ASSERT(p_Sides <= ONYX_MAX_REGULAR_POLYGON_SIDES && p_Sides >= 3,
                    "[ONYX] NGon sides must be between 3 and {}", ONYX_MAX_REGULAR_POLYGON_SIDES);
        return (D - 1) * 2 + p_Sides - 3 + (D - 2);
    }
};

template <Dimension D> struct Primitives;

/**
 * @brief Specialized primitive shapes for 2D rendering.
 *
 */
template <> struct Primitives<D2> : IPrimitives<D2>
{
    /// Total number of 2D primitives available.
    static constexpr usize AMOUNT = 2 + ONYX_REGULAR_POLYGON_COUNT;
};

/**
 * @brief Specialized primitive shapes for 3D rendering.
 *
 */
template <> struct Primitives<D3> : IPrimitives<D3>
{
    /// Total number of 3D primitives available.
    static constexpr usize AMOUNT = 5 + ONYX_REGULAR_POLYGON_COUNT;

    /**
     * @brief Get the index of the cube primitive.
     *
     * @return usize Index of the cube primitive.
     */
    static TKIT_CONSTEVAL usize GetCubeIndex() noexcept
    {
        return 2;
    }

    /**
     * @brief Get the index of the sphere primitive.
     *
     * @return usize Index of the sphere primitive.
     */
    static TKIT_CONSTEVAL usize GetSphereIndex() noexcept
    {
        return 3;
    }

    /**
     * @brief Get the index of the cylinder primitive.
     *
     * @return usize Index of the cylinder primitive.
     */
    static TKIT_CONSTEVAL usize GetCylinderIndex() noexcept
    {
        return 4;
    }
};

// Called automatically, user must not call this

/**
 * @brief Creates the combined primitive buffers.
 *
 * This function initializes the combined vertex and index buffers for all primitives.
 * It is called automatically and should not be called by the user.
 */
void CreateCombinedPrimitiveBuffers() noexcept;

} // namespace Onyx
