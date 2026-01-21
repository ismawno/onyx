#pragma once

#include "onyx/asset/mesh.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx::Assets
{
struct Specs
{
    u32 MaxStaticMeshes = 64;
};
void Initialize(const Specs &p_Specs);
void Terminate();

u32 GetStaticMeshBatchIndex(Mesh p_Mesh);
u32 GetStaticMeshIndexFromBatch(u32 p_Batch);
u32 GetCircleBatchIndex();

u32 GetBatchStart(GeometryType p_Geo);
u32 GetBatchEnd(GeometryType p_Geo);
u32 GetBatchCount(GeometryType p_Geo);
u32 GetBatchCount();

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &p_Data);
template <Dimension D> void UpdateMesh(Mesh p_Mesh, const StatMeshData<D> &p_Data);

template <Dimension D> MeshDataLayout GetStaticMeshLayout(Mesh p_Mesh);

template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshVertexBuffer();
template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshIndexBuffer();
template <Dimension D> u32 GetStaticMeshCount();

template <Dimension D> void Upload();
#ifdef ONYX_ENABLE_OBJ
template <Dimension D> VKit::Result<StatMeshData<D>> LoadStaticMesh(const char *p_Path);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateSquareMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 p_Sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> p_Vertices);

StatMeshData<D3> CreateCubeMesh();
StatMeshData<D3> CreateSphereMesh(u32 p_Rings, u32 p_Sectors);
StatMeshData<D3> CreateCylinderMesh(u32 p_Sides);

// move to renderer

} // namespace Onyx::Assets
