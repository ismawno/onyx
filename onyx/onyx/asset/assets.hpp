#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/asset/material.hpp"
#include "onyx/asset/sampler.hpp"
#include "onyx/asset/texture.hpp"
#include "onyx/asset/font.hpp"
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
#endif

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

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    TKit::Span<const CodePointRange> CharSets = FontCharSet_ASCII;
    u32v2 GlyphSize{32, 32};
    f32 SDFRange = 4.f;
    f32 Padding = 2.f;
    u32 AtlasWidth = 512;
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

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

Asset AddSampler(const SamplerData &data);
void UpdateSampler(Asset handle, const SamplerData &data);
void RemoveSampler(Asset handle);

template <Dimension D> GltfHandles AddGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfAssets<D> &data);

#ifdef ONYX_ENABLE_GLTF_LOAD

template <Dimension D>
ONYX_NO_DISCARD Result<GltfAssets<D>> LoadGltfAssetsFromFile(const std::string &path, LoadGltfDataFlags flags = 0);

ONYX_NO_DISCARD Result<TextureData> LoadTextureDataFromImageFile(
    const char *path, const ImageComponent requiredComponents = ImageComponent_Auto, LoadTextureDataFlags flags = 0);
#endif

template <Dimension D> ONYX_NO_DISCARD Result<AssetPool> CreateAssetPool(AssetPoolType ptype);
template <Dimension D> void DestroyAssetPool(AssetPool pool);

ONYX_NO_DISCARD Result<AssetPool> CreateFontPool();
void DestroyFontPool(AssetPool pool);

template <Dimension D> Asset AddMesh(AssetPool pool, const StatMeshData<D> &data);
template <Dimension D> Asset AddMesh(AssetPool pool, const ParaMeshData<D> &data);

Asset AddFont(AssetPool pool, const FontData &data);
Asset AddTexture(const TextureData &data, AddTextureFlags flags = 0);

template <Dimension D> void UpdateMesh(Asset handle, const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Asset handle, const ParaMeshData<D> &data);
void UpdateFont(Asset handle, const FontData &data);
void UpdateTexture(Asset handle, const TextureData &data, AddTextureFlags flags = 0);

void RemoveTexture(Asset handle);

#ifdef ONYX_ENABLE_FONT_LOAD
ONYX_NO_DISCARD Result<FontData> LoadFontFromFile(const char *path, const FontLoadOptions &opts = {});
#endif

template <Dimension D> Asset AddMaterial(AssetPool pool, const MaterialData<D> &data);
template <Dimension D> void UpdateMaterial(Asset handle, const MaterialData<D> &data);

template <Dimension D> StatMeshData<D> GetStaticMeshData(Asset handle);
template <Dimension D> ParaMeshData<D> GetParametricMeshData(Asset handle);
template <Dimension D> ParametricShape GetParametricShape(Asset handle);

template <Dimension D> const MaterialData<D> &GetMaterialData(Asset handle);
template <Dimension D> TKit::Span<const u32> GetAssetPoolIds(AssetPoolType ptype);

TKit::Span<const u32> GetFontPoolIds();

const SamplerData &GetSamplerData(Asset handle);
const TextureData &GetTextureData(Asset handle);
const FontData &GetFontData(Asset handle);
const GlyphData *GetGlyphData(Asset font, u32 codePoint);

u32 GetBatchCount();
template <Dimension D> u32 GetAssetCount(AssetPool pool);
template <Dimension D> MeshDataLayout GetMeshLayout(Asset handle);
template <Dimension D> const VKit::DeviceBuffer *GetMeshVertexBuffer(AssetPool pool);
template <Dimension D> const VKit::DeviceBuffer *GetMeshIndexBuffer(AssetPool pool);

const VKit::DeviceBuffer *GetGlyphVertexBuffer(AssetPool pool);
const VKit::DeviceBuffer *GetGlyphIndexBuffer(AssetPool pool);

u32 GetFontCount(AssetPool pool);
u32 GetGlyphCount(AssetPool pool);
MeshDataLayout GetGlyphLayout(Asset handle);

template <Dimension D> bool IsAssetValid(Asset handle, AssetType atype);
template <Dimension D> bool IsAssetPoolValid(Handle handle, AssetPoolType ptype);

bool IsAssetValid(Asset handle, AssetType atype);
bool IsAssetPoolValid(Handle handle, AssetPoolType ptype);

void Lock();
ONYX_NO_DISCARD Result<> Unlock();

ONYX_NO_DISCARD Result<bool> RequestUpload();
ONYX_NO_DISCARD Result<> Upload();

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D>
ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMeshFromObjFile(const char *path, const LoadObjDataFlags flags = 0);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh();
template <Dimension D> StatMeshData<D> CreateQuadMesh();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMesh(TKit::Span<const f32v2> vertices);

// rings and sectors should be even

StatMeshData<D3> CreateBoxMesh();
StatMeshData<D3> CreateSphereMesh(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMesh(u32 sides = 32);

template <Dimension D> ParaMeshData<D> CreateStadiumMesh();
template <Dimension D> ParaMeshData<D> CreateRoundedQuadMesh();

ParaMeshData<D3> CreateCapsuleMesh(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateRoundedBoxMesh(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateTorusMesh(u32 rings = 32, u32 sectors = 32);

} // namespace Onyx::Assets
