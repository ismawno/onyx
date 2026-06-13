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

template <Dimension D> MeshBuffers GetMeshBuffers(ResourcePool pool);
MeshBuffers GetFontBuffers(ResourcePool pool);
MeshBuffers GetGlyphBuffers(ResourcePool pool);

bool IsBackCulled(Resource handle);

u32 CombineSamplerTexIntoId(Resource sampler, Resource texture);
void UpdateTextureIdOffsetBuffer(VkCommandBuffer cmd);

Resource CreateMainRenderTexture(VkImageView view);
Resource CreateSecondaryRenderTexture(VkImageView view);

void UpdateTextureHandleOffset(Resource texture, Resource target);
void UpdateRenderTexture(Resource texture, VkImageView view);
} // namespace Onyx::Resources
