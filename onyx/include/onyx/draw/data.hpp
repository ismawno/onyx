#pragma once

#include "onyx/draw/vertex.hpp"
#include "onyx/buffer/index_buffer.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct ONYX_API IndexVertexData
{
    DynamicArray<Vertex<N>> Vertices;
    DynamicArray<Index> Indices;
};

ONYX_DIMENSION_TEMPLATE IndexVertexData<N> Load(std::string_view p_Path) noexcept;

} // namespace ONYX