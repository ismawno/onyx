#pragma once

#include "onyx/core/core.hpp"
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

#ifdef ONYX_ENABLE_OBJ_LOAD
using LoadObjDataFlags = u8;
enum LoadObjDataFlagBit : LoadObjDataFlags
{
    LoadObjDataFlag_CenterVerticesAroundOrigin = 1 << 0,
};

template <Dimension D>
ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMeshDataFromObjFile(const char *path,
                                                                      const LoadObjDataFlags flags = 0);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMeshData();
template <Dimension D> StatMeshData<D> CreateQuadMeshData();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMeshData(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMeshData(TKit::Span<const f32v2> vertices);

// rings and sectors should be even

StatMeshData<D3> CreateBoxMeshData();
StatMeshData<D3> CreateSphereMeshData(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMeshData(u32 sides = 32);

template <Dimension D> ParaMeshData<D> CreateStadiumMeshData();
template <Dimension D> ParaMeshData<D> CreateRoundedQuadMeshData();

ParaMeshData<D3> CreateCapsuleMeshData(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateRoundedBoxMeshData(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateTorusMeshData(u32 rings = 32, u32 sectors = 32);

} // namespace Onyx
