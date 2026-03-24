#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"
#include "tkit/container/dynamic_array.hpp"

namespace Onyx
{
template <typename Vertex> struct MeshData
{
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
};

template <Dimension D> struct MeshData<ParaVertex<D>>
{
    TKit::DynamicArray<ParaVertex<D>> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    ParametricShape Shape{};
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;
template <Dimension D> using ParaMeshData = MeshData<ParaVertex<D>>;

struct MeshDataLayout
{
    u32 VertexStart = 0;
    u32 VertexCount = 0;
    u32 IndexStart = 0;
    u32 IndexCount = 0;
};

} // namespace Onyx
