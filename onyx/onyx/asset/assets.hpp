#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/asset/material.hpp"
#include "onyx/asset/sampler.hpp"
#include "onyx/asset/texture.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx
{
using AddTextureFlags = u8;
enum AddTextureFlagBit : AddTextureFlags
{
    AddTextureFlag_ManuallyHandledMemory = 1 << 0,
};

#ifdef ONYX_ENABLE_OBJ_LOAD
using LoadObjDataFlags = u8;
enum LoadObjDataFlagBit : LoadObjDataFlags
{
    LoadObjDataFlag_CenterVerticesAroundOrigin = 1 << 0,
};
#endif

#ifdef ONYX_ENABLE_GLTF_LOAD
using LoadGltfDataFlags = u8;
enum LoadGltfDataFlagBit : LoadGltfDataFlags
{
    LoadGltfDataFlag_ForceRGBA = 1 << 0,
    LoadGltfDataFlag_CenterVerticesAroundOrigin = 1 << 1,
};

using LoadTextureDataFlags = u8;
enum LoadTextureDataFlagBit : LoadTextureDataFlags
{
    LoadTextureDataFlag_AsLinearImage = 1 << 0,
};
template <Dimension D> struct GltfAssets
{
    TKit::TierArray<StatMeshData<D>> StaticMeshes{};
    // here texture handles inside materials refer to the Textures attribute in GltfAssets, not to any real Asset
    // handle!! AddGltfAssets modifies this material data so that it actually points to real textures (thats why it is
    // taken as a non-const lvalue)
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<SamplerData> Samplers{};
    TKit::TierArray<TextureData> Textures{};
};

struct GltfHandles
{
    TKit::TierArray<Asset> StaticMeshes{};
    TKit::TierArray<Asset> Materials{};
    TKit::TierArray<Asset> Samplers{};
    TKit::TierArray<Asset> Textures{};
};
#endif

enum ImageComponent : u8
{
    ImageComponent_Auto = 0,
    ImageComponent_Grey = 1,
    ImageComponent_GreyAlpha = 2,
    ImageComponent_RGB = 3,
    ImageComponent_RGBA = 4,
};
} // namespace Onyx

namespace Onyx::Assets
{
struct Specs
{
    u32 MaxTextures = 1024;
    u32 MaxSamplers = 8;
};

void Initialize(const Specs &specs);
void Terminate();

Asset AddSampler(const SamplerData &data);
void UpdateSampler(Asset handle, const SamplerData &data);
void RemoveSampler(Asset handle);

#ifdef ONYX_ENABLE_GLTF_LOAD

template <Dimension D> GltfHandles AddGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfAssets<D> &data);

Asset AddTexture(const TextureData &data, AddTextureFlags flags = 0);
void UpdateTexture(Asset handle, const TextureData &data, AddTextureFlags flags = 0);
void RemoveTexture(Asset handle);
#else
Asset AddTexture(const TextureData &data, AddTAddTextureFlags flags = AddTextureFlag_ManuallyHandledMemory);
void UpdateTexture(Asset handle, const TextureData &data,
                   AddTAddTextureFlags flags = AddTextureFlag_ManuallyHandledMemory);
#endif

template <Dimension D> bool IsMeshPoolHandleValid(Geometry geo, AssetPool handle);
template <Dimension D> bool IsMeshHandleValid(Geometry geo, Asset handle);
template <Dimension D> bool IsMaterialPoolHandleValid(AssetPool handle);
template <Dimension D> bool IsMaterialHandleValid(Asset handle);

bool IsSamplerHandleValid(Asset handle);
bool IsTextureHandleValid(Asset handle);

template <Dimension D> ONYX_NO_DISCARD Result<AssetPool> CreateMeshPool(Geometry geo);
template <Dimension D> void DestroyMeshPool(Geometry geo, AssetPool pool);

template <Dimension D> Asset AddMesh(AssetPool pool, const StatMeshData<D> &data);
template <Dimension D> Asset AddMesh(AssetPool pool, const ParaMeshData<D> &data);

template <Dimension D> void UpdateMesh(Asset handle, const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Asset handle, const ParaMeshData<D> &data);

template <Dimension D> ONYX_NO_DISCARD Result<AssetPool> CreateMaterialPool();
template <Dimension D> void DestroyMaterialPool(AssetPool handle);

template <Dimension D> Asset AddMaterial(AssetPool pool, const MaterialData<D> &data);
template <Dimension D> void UpdateMaterial(Asset handle, const MaterialData<D> &data);

// only valid for meshes and materials
inline u32 GetAssetIndex(const Asset handle)
{
    return handle & 0x00FFFFFF;
}
inline AssetPool GetPoolHandle(const Asset handle)
{
    return AssetPool(handle >> 24);
}
inline Asset GetAssetHandle(const AssetPool pool, const u32 assetIdx)
{
    return (u32(pool) << 24) | assetIdx;
}

template <Dimension D> StatMeshData<D> GetStaticMeshData(Asset handle);
template <Dimension D> ParaMeshData<D> GetParametricMeshData(Asset handle);
template <Dimension D> ParametricShape GetParametricShape(Asset handle);

template <Dimension D> const MaterialData<D> &GetMaterialData(Asset handle);
const TextureData &GetTextureData(Asset handle);

template <Dimension D> TKit::Span<const u32> GetMeshAssetPools(Geometry geo);

u32 GetBatchCount();
template <Dimension D> u32 GetMeshCount(Geometry geo, AssetPool pool);
template <Dimension D> MeshDataLayout GetMeshLayout(Geometry geo, Asset handle);
template <Dimension D> const VKit::DeviceBuffer *GetMeshVertexBuffer(Geometry geo, AssetPool pool);
template <Dimension D> const VKit::DeviceBuffer *GetMeshIndexBuffer(Geometry geo, AssetPool pool);

void Lock();
ONYX_NO_DISCARD Result<> Unlock();

ONYX_NO_DISCARD Result<bool> RequestUpload();
ONYX_NO_DISCARD Result<> Upload();

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D>
ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMeshFromObjFile(const char *path, const LoadObjDataFlags flags = 0);
#endif
#ifdef ONYX_ENABLE_GLTF_LOAD

template <Dimension D>
ONYX_NO_DISCARD Result<GltfAssets<D>> LoadGltfAssetsFromFile(const std::string &path, LoadGltfDataFlags flags = 0);

// template <Dimension D>
// ONYX_NO_DISCARD Result<GltfData<D>> LoadGltfSceneFromFile(const std::string &path, LoadGltfDataFlags flags = 0);

ONYX_NO_DISCARD Result<TextureData> LoadTextureDataFromImageFile(
    const char *path, const ImageComponent requiredComponents = ImageComponent_Auto, LoadTextureDataFlags flags = 0);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateQuadMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> vertices);

StatMeshData<D3> CreateBoxMesh();
StatMeshData<D3> CreateSphereMesh(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMesh(u32 sides = 32);

template <Dimension D> ParaMeshData<D> CreateStadiumMesh(u32 sides = 16);

} // namespace Onyx::Assets
