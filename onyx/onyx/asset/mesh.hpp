#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"
#include "onyx/asset/handle.hpp"
#include "tkit/container/dynamic_array.hpp"

namespace Onyx
{
using Mesh = Asset;
constexpr Mesh NullMesh = NullAsset;

template <typename Vertex> struct MeshData
{
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;

struct MeshDataLayout
{
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
};

} // namespace Onyx
