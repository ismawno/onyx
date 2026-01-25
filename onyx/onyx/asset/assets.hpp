#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx::Assets
{
struct Specs
{
    u32 MaxStaticMeshes = 64;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

u32 GetStaticMeshBatchIndex(Mesh mesh);
u32 GetStaticMeshIndexFromBatch(u32 batch);
u32 GetCircleBatchIndex();

u32 GetBatchStart(GeometryType geo);
u32 GetBatchEnd(GeometryType geo);
u32 GetBatchCount(GeometryType geo);
u32 GetBatchCount();

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Mesh mesh, const StatMeshData<D> &data);

template <Dimension D> MeshDataLayout GetStaticMeshLayout(Mesh mesh);

template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshVertexBuffer();
template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshIndexBuffer();
template <Dimension D> u32 GetStaticMeshCount();

template <Dimension D> ONYX_NO_DISCARD Result<> Upload();
#ifdef ONYX_ENABLE_OBJ
template <Dimension D> ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMesh(const char *path);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateSquareMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> vertices);

StatMeshData<D3> CreateCubeMesh();
StatMeshData<D3> CreateSphereMesh(u32 rings, u32 sectors);
StatMeshData<D3> CreateCylinderMesh(u32 sides);

// move to renderer

} // namespace Onyx::Assets
