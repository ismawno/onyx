#pragma once

#include "onyx/resources.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "vkit/resource/device_image.hpp"

namespace Onyx::Resources
{
void Initialize(const Specs &specs);
void Terminate();

struct MeshBuffers
{
    const VKit::DeviceBuffer *VertexBuffer = nullptr;
    const VKit::DeviceBuffer *IndexBuffer = nullptr;
};

template <Dimension D> MeshBuffers ResourcePool_GetMeshBuffers(ResourcePool pool);
MeshBuffers FontPool_GetFontBuffers(ResourcePool pool);
MeshBuffers FontPool_GetGlyphBuffers(ResourcePool pool);

bool IsBackCulled(Resource handle);

u32 CombineSamplerTexIntoId(Resource sampler, Resource texture);
void UpdateTextureIdOffsetBuffer(VkCommandBuffer cmd);

Resource Texture_CreateMainRenderTexture(VkImageView view);
Resource Texture_CreateSecondaryRenderTexture(VkImageView view);

void Texture_UpdateHandleOffset(Resource texture, Resource target);
void Texture_UpdateRenderTexture(Resource texture, VkImageView view);
} // namespace Onyx::Resources
