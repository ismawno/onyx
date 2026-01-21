#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"

namespace Onyx
{
using Mesh = u32;
constexpr Mesh NullMesh = TKit::Limits<Mesh>::Max();

template <typename Vertex> struct MeshData
{
    TKit::TierArray<Vertex> Vertices;
    TKit::TierArray<Index> Indices;
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;

struct MeshDataLayout
{
    u32 VertexStart;
    u32 IndexStart;
    u32 IndexCount;
};

} // namespace Onyx
