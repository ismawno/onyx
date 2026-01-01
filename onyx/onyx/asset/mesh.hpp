#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"

namespace Onyx
{
using Mesh = u32;
constexpr Mesh NullMesh = TKit::Limits<Mesh>::Max();

template <typename Vertex> struct MeshData
{
    HostBuffer<Vertex> Vertices;
    HostBuffer<Index> Indices;
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;
} // namespace Onyx
