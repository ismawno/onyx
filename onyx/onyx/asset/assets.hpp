#pragma once

#include "onyx/asset/mesh.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Assets
{
void Initialize();
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout();
const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout();

VkDescriptorSet WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                VkDescriptorSet p_OldSet = VK_NULL_HANDLE);

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &p_Data);
template <Dimension D> void UpdateMesh(Mesh p_Mesh, const StatMeshData<D> &p_Data);

template <Dimension D> u32 GetStaticMeshCount();

template <Dimension D> void Upload();
template <Dimension D> void BindStaticMeshes(VkCommandBuffer p_CommandBuffer);
template <Dimension D>
void DrawStaticMesh(VkCommandBuffer p_CommandBuffer, Mesh p_Mesh, u32 p_FirstInstance, u32 p_InstanceCount);

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

} // namespace Onyx::Assets
