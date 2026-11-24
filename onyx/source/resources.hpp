#pragma once

#include "onyx/resources.hpp"
#include "onyx/onyx.hpp"

namespace Onyx
{
void Resources_Initialize();
void Resources_Terminate();

struct MeshBuffers
{
    Graph::Buffer VertexBuffer = Graph::NullHandle;
    Graph::Buffer IndexBuffer = Graph::NullHandle;
};

template <Dimension D> MeshBuffers ResourcePool_GetMeshBuffers(ResourcePool pool);
MeshBuffers FontPool_GetFontBuffers(ResourcePool pool);
MeshBuffers FontPool_GetGlyphBuffers(ResourcePool pool);

template <Dimension D> TKit::Span<const u32> ResourcePool_GetIds(ResourceType rtype);
TKit::Span<const u32> FontPool_GetIds();

u32 Resources_CombineSamplerTexIntoId(Resource sampler, Resource texture);
void Resources_UpdateTextureIdOffsetBuffer(Graph::CommandBuffer cmd);

Resource Texture_CreateMainRenderTexture(Graph::ImageView view);
Resource Texture_CreateSecondaryRenderTexture(Graph::ImageView view);

void Texture_UpdateHandleOffset(Resource texture, Resource target);
void Texture_UpdateRenderTexture(Resource texture, Graph::ImageView view);

template <Dimension D> u32 Resources_GetDistinctBatchDrawCount();

#ifdef ONYX_ENABLE_HOOKS
struct HookShaders
{
    Graph::Shader Vertex = Graph::NullHandle;
    Graph::Shader Opaque = Graph::NullHandle;
    Graph::Shader Transparent = Graph::NullHandle;
};
template <Dimension D> HookShaders Hook_GetFromRenderState(RenderPass rpass, Geometry geo, RenderState state);
void Hook_ResetAll(const ShaderCompileOptions &opts);
#endif
} // namespace Onyx
