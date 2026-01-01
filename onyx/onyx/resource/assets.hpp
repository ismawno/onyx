#pragma once

#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"
#include "vkit/descriptors/descriptor_pool.hpp"

namespace Onyx::Assets
{
using Mesh = u32;
constexpr Mesh NullMesh = TKit::Limits<Mesh>::Max();

template <typename Vertex> struct MeshData
{
    HostBuffer<Vertex> Vertices;
    HostBuffer<Index> Indices;
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;

ONYX_API void Initialize();
ONYX_API void Terminate();

ONYX_API const VKit::DescriptorPool &GetDescriptorPool();
ONYX_API const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout();
ONYX_API const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout();

ONYX_API VkDescriptorSet WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                         VkDescriptorSet p_OldSet = VK_NULL_HANDLE);

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &p_Data);
template <Dimension D> void UpdateMesh(Mesh p_Mesh, const StatMeshData<D> &p_Data);

template <Dimension D> u32 GetStaticMeshCount();

template <Dimension D> void Upload();
template <Dimension D> void BindStaticMeshes(VkCommandBuffer p_CommandBuffer);
template <Dimension D>
void DrawStaticMesh(VkCommandBuffer p_CommandBuffer, Mesh p_Mesh, u32 p_FirstInstance, u32 p_InstanceCount);

#ifdef ONYX_ENABLE_OBJ
template <Dimension D> VKit::Result<StatMeshData<D>> LoadStaticMesh(std::string_view p_Path);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateSquareMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 p_Sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> p_Vertices);

ONYX_API StatMeshData<D3> CreateCubeMesh();
ONYX_API StatMeshData<D3> CreateSphereMesh(u32 p_Rings, u32 p_Sectors);
ONYX_API StatMeshData<D3> CreateCylinderMesh(u32 p_Sides);

} // namespace Onyx::Assets
