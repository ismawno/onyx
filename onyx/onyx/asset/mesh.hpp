#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"
#include "tkit/container/tier_array.hpp"

namespace Onyx
{
template <typename Vertex> struct MeshData
{
    TKit::TierArray<Vertex> Vertices{};
    TKit::TierArray<Index> Indices{};
};

template <Dimension D> struct MeshData<ParaVertex<D>>
{
    TKit::TierArray<ParaVertex<D>> Vertices{};
    TKit::TierArray<Index> Indices{};
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
