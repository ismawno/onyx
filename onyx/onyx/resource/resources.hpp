#pragma once

#include "onyx/core/core.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/resource/handle.hpp"
#include "onyx/resource/image.hpp"
#include "onyx/resource/sampler.hpp"
#include "onyx/resource/mesh.hpp"
#include "onyx/resource/gltf.hpp"
#include "onyx/resource/font.hpp"
#include "onyx/resource/material.hpp"
#include "vkit/resource/device_image.hpp"
#include "vkit/resource/sampler.hpp"

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
struct Specs
{
    u32 MaxBuffers = 1024;
    u32 MaxSamplers = 8;
    u32 MaxStandaloneSamplers = 8;
    u32 MaxImages = 512;
    u32 MaxTextures = 1024;
    u32 MaxMaterials = 256;
    u32 MaxBounds = 1024;
};

void Initialize(const Specs &specs);
void Terminate();

Resource CreateBuffer(const VKit::DeviceBuffer::Builder &builder);
inline Resource CreateBuffer(const VKit::DeviceBufferFlags flags, const VkDeviceSize size)
{
    return CreateBuffer(VKit::DeviceBuffer::Builder(GetDevice(), GetVulkanAllocator(), flags).SetSize(size));
}
template <typename T>
Resource CreateBuffer(const VKit::DeviceBufferFlags flags, const u32 capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    return CreateBuffer(flags, capacity * sizeof(T));
}

const VKit::DeviceBuffer &GetBuffer(Resource buffer);
void DestroyBuffer(Resource buffer);
void ReleaseBuffer(Resource buffer);

Resource CreateSampler(const VKit::Sampler::Builder &builder);
Resource CreateSampler(const SamplerData &data);
void DestroySampler(Resource sampler);
void ReleaseSampler(Resource sampler);

const VKit::Sampler &GetSampler(Resource sampler);

Resource CreateImage(const VKit::DeviceImage::Builder &builder);
Resource CreateImage(const ImageData &data);
void DestroyImage(Resource image);
void ReleaseImage(Resource image);

const VKit::DeviceImage &GetImage(const Resource image);

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

struct MeshBuffers
{
    const VKit::DeviceBuffer *VertexBuffer = nullptr;
    const VKit::DeviceBuffer *IndexBuffer = nullptr;
};

template <Dimension D> MeshBuffers GetMeshBuffers(ResourcePool pool);
MeshBuffers GetFontBuffers(ResourcePool pool);
MeshBuffers GetGlyphBuffers(ResourcePool pool);

u32 GetFontCount(ResourcePool pool);
u32 GetGlyphCount(ResourcePool pool);

MeshDataLayout GetFontLayout(Resource font);
MeshDataLayout GetGlyphLayout(Resource glyph);

template <Dimension D> bool IsResourceValid(Resource handle, ResourceType atype);
template <Dimension D> bool IsResourcePoolValid(Handle handle, ResourceType atype);

bool IsResourceValid(Resource handle, ResourceType atype);
bool IsResourcePoolValid(ResourcePool handle, ResourceType atype);

void Sync(SyncFlags flags);
bool RequestSync(SyncFlags flags);

void LockSync();
void UnlockSync();
bool IsSyncLocked();

} // namespace Onyx::Resources
