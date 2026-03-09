#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/asset/material.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/resource/sampler.hpp"

namespace Onyx
{
using AssetsFlags = u8;
enum AssetsFlagBit : AssetsFlags
{
    AssetsFlag_Locked = 1 << 0,
    AssetsFlag_MustUpload = 1 << 1,
    AssetsFlag_UserHandledMemory = 1 << 2,
};
} // namespace Onyx

namespace Onyx::Assets
{
struct Specs
{
    u32 MaxStaticMeshes = 64;
    u32 MaxMaterials = 1024;
    u32 MaxTextures = 1024;
    u32 MaxSamplers = 8;
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

u32 GetStaticMeshBatchIndex(Mesh mesh);
u32 GetStaticMeshIndexFromBatch(u32 batch);
u32 GetCircleBatchIndex();

u32 GetBatchStart(Geometry geo);
u32 GetBatchEnd(Geometry geo);
u32 GetBatchCount(Geometry geo);
u32 GetBatchCount();

void AddSampler(const VKit::Sampler &sampler);
ONYX_NO_DISCARD Result<> AddDefaultSampler();

#ifdef ONYX_ENABLE_GLTF_LOAD
Texture AddTexture(const TextureData &data, const AssetsFlags flags = 0);
void UpdateTexture(Texture tex, const TextureData &data, const AssetsFlags flags = 0);
#else
Texture AddTexture(const TextureData &data, const AssetsFlags flags = AssetsFlag_UserHandledMemory);
void UpdateTexture(Texture tex, const TextureData &data, const AssetsFlags flags = AssetsFlag_UserHandledMemory);
#endif

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Mesh mesh, const StatMeshData<D> &data);

template <Dimension D> Material AddMaterial(const MaterialData<D> &data);
template <Dimension D> void UpdateMaterial(Material material, const MaterialData<D> &data);

template <Dimension D> MeshDataLayout GetStaticMeshLayout(Mesh mesh);

template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshVertexBuffer();
template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshIndexBuffer();
template <Dimension D> u32 GetStaticMeshCount();

void Lock();
ONYX_NO_DISCARD Result<> Unlock();

ONYX_NO_DISCARD Result<bool> RequestUpload();
ONYX_NO_DISCARD Result<> Upload();

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D> ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMeshFromObjFile(const char *path);
#endif
#ifdef ONYX_ENABLE_GLTF_LOAD
ONYX_NO_DISCARD Result<TextureData> LoadTextureDataFromImageFile(const char *path);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateSquareMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> vertices);

StatMeshData<D3> CreateCubeMesh();
StatMeshData<D3> CreateSphereMesh(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMesh(u32 sides);

// move to renderer

} // namespace Onyx::Assets
