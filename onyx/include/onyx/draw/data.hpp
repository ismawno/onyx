#pragma once

#include "onyx/draw/vertex.hpp"
#include "onyx/buffer/index_buffer.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
struct ONYX_API IndexVertexData
{
    DynamicArray<Vertex<N>> Vertices;
    DynamicArray<Index> Indices;
};

template <u32 N>
    requires(IsDim<N>())
IndexVertexData<N> Load(std::string_view p_Path) noexcept;

} // namespace ONYX