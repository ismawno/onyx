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
    AssetsFlag_Locked = 1 << 0,             // internal
    AssetsFlag_MustUpload = 1 << 1,         // internal
    AssetsFlag_UserHandledMemory = 1 << 2,  // relevant in AddTexture
    AssetsFlag_LoadImageForceRGBA = 1 << 3, // relevant when loading gltf
    AssetsFlag_LoadAsLinearImage = 1 << 4,  // relevant when loading image
    // AssetsFlag_IncludeSampler = 1 << 5,     // relevant when loading gltf
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

Sampler AddSampler(const VKit::Sampler &sampler);
ONYX_NO_DISCARD Result<Sampler> AddDefaultSampler();

#ifdef ONYX_ENABLE_GLTF_LOAD
// samplers are not supported for now
template <Dimension D> struct GltfData
{
    TKit::TierArray<StatMeshData<D>> StaticMeshes{};
    // here texture handles refer to the Textures attribute in GltfData, not to any Asset handle!!
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<TextureData> Textures{};
};

struct GltfHandles
{
    TKit::TierArray<Mesh> StaticMeshes{};
    TKit::TierArray<Material> Materials{};
    TKit::TierArray<Texture> Textures{};
};

template <Dimension D> GltfHandles AddGltfData(const GltfData<D> &data, Sampler defaultSampler = NullSampler);

Texture AddTexture(const TextureData &data, AssetsFlags flags = 0);
void UpdateTexture(Texture tex, const TextureData &data, AssetsFlags flags = 0);
#else
Texture AddTexture(const TextureData &data, AssetsFlags flags = AssetsFlag_UserHandledMemory);
void UpdateTexture(Texture tex, const TextureData &data, AssetsFlags flags = AssetsFlag_UserHandledMemory);
#endif

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Mesh mesh, const StatMeshData<D> &data);

template <Dimension D> Material AddMaterial(const MaterialData<D> &data);
template <Dimension D> void UpdateMaterial(Material material, const MaterialData<D> &data);

template <Dimension D> StatMeshData<D> GetStaticMeshData(Mesh mesh);
template <Dimension D> const MaterialData<D> &GetMaterialData(Material material);
const TextureData &GetTextureData(Texture texture);

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
template <Dimension D> ONYX_NO_DISCARD Result<GltfData<D>> LoadGltfFile(const char *path, AssetsFlags flags);
ONYX_NO_DISCARD Result<TextureData> LoadTextureDataFromImageFile(const char *path, const u32 requiredComponents = 0,
                                                                 AssetsFlags flags = 0);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateSquareMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> vertices);

StatMeshData<D3> CreateCubeMesh();
StatMeshData<D3> CreateSphereMesh(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMesh(u32 sides = 32);

// move to renderer

} // namespace Onyx::Assets
