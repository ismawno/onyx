#pragma once

#include "onyx/resources.hpp"

namespace Onyx::Resources
{
void Initialize(const Specs &specs);
void Terminate();

struct MeshBuffers
{
    Graph::Buffer VertexBuffer = Graph::NullHandle;
    Graph::Buffer IndexBuffer = Graph::NullHandle;
};

template <Dimension D> MeshBuffers ResourcePool_GetMeshBuffers(ResourcePool pool);
MeshBuffers FontPool_GetFontBuffers(ResourcePool pool);
MeshBuffers FontPool_GetGlyphBuffers(ResourcePool pool);

bool IsBackCulled(Resource handle);

u32 CombineSamplerTexIntoId(Resource sampler, Resource texture);
void UpdateTextureIdOffsetBuffer(Graph::CommandBuffer cmd);

Resource Texture_CreateMainRenderTexture(Graph::ImageView view);
Resource Texture_CreateSecondaryRenderTexture(Graph::ImageView view);

void Texture_UpdateHandleOffset(Resource texture, Resource target);
void Texture_UpdateRenderTexture(Resource texture, Graph::ImageView view);
} // namespace Onyx::Resources
