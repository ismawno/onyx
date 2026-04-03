#pragma once

#include "onyx/asset/gltf.hpp"
#include "onyx/asset/font.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx
{
using CreateTextureFlags = u8;
enum CreateTextureFlagBit : CreateTextureFlags
{
    CreateTextureFlag_ManuallyHandledMemory = 1 << 0,
};

} // namespace Onyx

namespace Onyx::Assets
{
struct Specs
{
    u32 MaxMaterials = 256;
    u32 MaxBounds = 1024;
    u32 MaxTextures = 1024;
    u32 MaxSamplers = 8;
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

template <Dimension D> ONYX_NO_DISCARD Result<AssetPool> CreateAssetPool(AssetType atype);
ONYX_NO_DISCARD Result<AssetPool> CreateFontPool();

Asset CreateSampler(const SamplerData &data);
Asset CreateTexture(const TextureData &data, CreateTextureFlags flags = 0);

template <Dimension D> Asset CreateMesh(AssetPool pool, const StatMeshData<D> &data);
template <Dimension D> Asset CreateMesh(AssetPool pool, const ParaMeshData<D> &data);
template <Dimension D> Asset CreateMaterial(const MaterialData<D> &data);

Asset CreateFont(AssetPool pool, const FontData &data);
template <Dimension D> GltfHandles CreateGltfAssets(AssetPool meshPool, GltfData<D> &data);

void UpdateSampler(Asset sampler, const SamplerData &data);
void UpdateTexture(Asset texture, const TextureData &data, CreateTextureFlags flags = 0);

template <Dimension D> void UpdateMesh(Asset mesh, const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Asset mesh, const ParaMeshData<D> &data);
template <Dimension D> void UpdateMaterial(Asset material, const MaterialData<D> &data);

void UpdateFont(Asset font, const FontData &data);

template <Dimension D> void DestroyAssetPool(AssetPool pool);
void DestroyFontPool(AssetPool pool);

void DestroySampler(Asset sampler);
void DestroyTexture(Asset texture);

template <Dimension D> void DestroyMaterial(Asset material);

template <Dimension D> StatMeshData<D> GetStaticMeshData(Asset mesh);
template <Dimension D> ParaMeshData<D> GetParametricMeshData(Asset mesh);
template <Dimension D> ParametricShape GetParametricShape(Asset mesh);

template <Dimension D> const MaterialData<D> &GetMaterialData(Asset material);
template <Dimension D> TKit::Span<const u32> GetAssetPoolIds(AssetType atype);

TKit::Span<const u32> GetFontPoolIds();

const SamplerData &GetSamplerData(Asset sampler);
const TextureData &GetTextureData(Asset texture);
const FontData &GetFontData(Asset font);

Asset GetFontAtlas(Asset font);
const Glyph *GetGlyph(Asset font, u32 codePoint);

template <Dimension D> u32 GetDistinctBatchDrawCount();

template <Dimension D> u32 GetAssetCount(AssetPool pool);

template <Dimension D> MeshDataLayout GetMeshLayout(Asset mesh);
template <Dimension D> Asset GetMeshBounds(Asset mesh);
template <Dimension D> const BoundsData<D> &GetBoundsData(Asset bounds);

struct MeshBuffers
{
    const VKit::DeviceBuffer *VertexBuffer = nullptr;
    const VKit::DeviceBuffer *IndexBuffer = nullptr;
};

template <Dimension D> MeshBuffers GetMeshBuffers(AssetPool pool);
MeshBuffers GetFontBuffers(AssetPool pool);
MeshBuffers GetGlyphBuffers(AssetPool pool);

u32 GetFontCount(AssetPool pool);
u32 GetGlyphCount(AssetPool pool);

MeshDataLayout GetFontLayout(Asset font);
MeshDataLayout GetGlyphLayout(Asset glyph);

template <Dimension D> bool IsAssetValid(Asset handle, AssetType atype);
template <Dimension D> bool IsAssetPoolValid(Handle handle, AssetType atype);

bool IsAssetValid(Asset handle, AssetType atype);
bool IsAssetPoolValid(Handle handle, AssetType atype);

void Lock();
ONYX_NO_DISCARD Result<> Unlock();

ONYX_NO_DISCARD Result<bool> RequestUpload();
ONYX_NO_DISCARD Result<> Upload();

} // namespace Onyx::Assets
