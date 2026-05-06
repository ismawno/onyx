#pragma once

#include "onyx/resource/handle.hpp"
#include "onyx/resource/image.hpp"
#include "onyx/resource/sampler.hpp"
#include "onyx/resource/mesh.hpp"
#include "onyx/resource/gltf.hpp"
#include "onyx/resource/font.hpp"
#include "onyx/resource/material.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
using SyncFlags = u8;
enum SyncFlagBit : SyncFlags
{
    SyncFlag_StaticMeshes = 1 << 0,
    SyncFlag_ParametricMeshes = 1 << 1,
    SyncFlag_Materials = 1 << 2,
    SyncFlag_Fonts = 1 << 3,
    SyncFlag_Samplers = 1 << 4,
    SyncFlag_Images = 1 << 5,
    SyncFlag_All = TKIT_U8_MAX,
};
} // namespace Onyx

namespace Onyx::Resources
{
// NOTE(Isma): No way to create/use buffers in onyx yet

void DestroyBuffer(Resource buffer);
void ReleaseBuffer(Resource buffer);

Resource CreateSampler(const SamplerData &data);
void DestroySampler(Resource sampler);
void ReleaseSampler(Resource sampler);

Resource CreateImage(const ImageData &data);
void DestroyImage(Resource image);
void ReleaseImage(Resource image);

Resource CreateTexture(Resource image, VkImageView imageView);

// TODO(Isma): Add options for the texture: mips, array layers, etc
Resource CreateTexture(Resource image, u32 viewIndex = TKIT_U32_MAX);
void DestroyTexture(Resource texture);

template <Dimension D> Resource RegisterMesh(ResourcePool pool, const StatMeshData<D> &data);
template <Dimension D> Resource RegisterMesh(ResourcePool pool, const ParaMeshData<D> &data);
template <Dimension D> Resource RegisterMaterial(const MaterialData<D> &data);

Resource RegisterFont(ResourcePool pool, const FontData &data);
template <Dimension D> GltfHandles RegisterGltfResources(ResourcePool meshPool, GltfData<D> &data);

template <Dimension D> void UpdateMesh(Resource mesh, const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Resource mesh, const ParaMeshData<D> &data);
template <Dimension D> void UpdateMaterial(Resource material, const MaterialData<D> &data);

template <Dimension D> ResourcePool CreateResourcePool(ResourceType atype);
ResourcePool CreateFontPool();

template <Dimension D> void DestroyResourcePool(ResourcePool pool);
template <Dimension D> void ReleaseResourcePool(ResourcePool pool);

void DestroyFontPool(ResourcePool pool);
void ReleaseFontPool(ResourcePool pool);

template <Dimension D> void DestroyMaterial(Resource material);

template <Dimension D> StatMeshData<D> GetStaticMeshData(Resource mesh);
template <Dimension D> ParaMeshData<D> GetParametricMeshData(Resource mesh);
template <Dimension D> ParametricShape GetParametricShape(Resource mesh);

template <Dimension D> const MaterialData<D> &GetMaterialData(Resource material);
template <Dimension D> TKit::Span<const u32> GetResourcePoolIds(ResourceType atype);

TKit::Span<const u32> GetFontPoolIds();
const FontData &GetFontData(Resource font);

Resource GetFontAtlas(Resource font);
const Glyph *GetGlyph(Resource font, u32 codePoint);

template <Dimension D> u32 GetDistinctBatchDrawCount();

template <Dimension D> u32 GetResourceCount(ResourcePool pool);

template <Dimension D> MeshDataLayout GetMeshLayout(Resource mesh);
template <Dimension D> Resource GetMeshBounds(Resource mesh);
template <Dimension D> const BoundsData<D> &GetBoundsData(Resource bounds);

u32 GetFontCount(ResourcePool pool);
u32 GetGlyphCount(ResourcePool pool);

MeshDataLayout GetFontLayout(Resource font);
MeshDataLayout GetGlyphLayout(Resource glyph);

template <Dimension D> bool IsResourceValid(Resource handle, ResourceType atype);
template <Dimension D> bool IsResourcePoolValid(Handle handle, ResourceType atype);

bool IsResourceValid(Resource handle, ResourceType atype);
bool IsResourcePoolValid(ResourcePool handle, ResourceType atype);

void Sync(SyncFlags flags);

} // namespace Onyx::Resources
