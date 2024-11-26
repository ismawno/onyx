#pragma once

#include "onyx/draw/vertex.hpp"
#include "onyx/buffer/index_buffer.hpp"

namespace Onyx
{
template <Dimension D> struct ONYX_API IndexVertexData
{
    DynamicArray<Vertex<D>> Vertices;
    DynamicArray<Index> Indices;
};

template <Dimension D> IndexVertexData<D> Load(std::string_view p_Path) noexcept;

} // namespace Onyx