#pragma once

#include "onyx/core.hpp"
#include "onyx/window.hpp"
#include "onyx/render_texture.hpp"
#include "onyx/instance.hpp"
#include "pass.hpp"
#include "graph/descriptor.hpp"

namespace Onyx
{
template <Dimension D> class RenderContext;

struct TransferSubmitInfo
{
    Graph::CommandBuffer Command = Graph::NullHandle;
    u64 InFlightValue = 0;

    operator bool() const
    {
        return Command != Graph::NullHandle;
    }
};

struct RenderSubmitInfo
{
    Graph::CommandBuffer Command = Graph::NullHandle;
    Graph::Swapchain Swapchain = Graph::NullHandle;
    TKit::TierArray<Graph::WaitInfo> WaitValues{};
    u64 InFlightValue = 0;
};

using RenderFlags = u8;
enum RenderFlagBit : RenderFlags
{
#ifdef ONYX_ENABLE_IMGUI
    RenderFlag_ImGui = 1U << 0,
#endif
};

void Renderer_Initialize();
void Renderer_Terminate();

template <Dimension D> RenderContext<D> *Renderer_CreateContext(u32 immediateDynamicMeshCapacity);
template <Dimension D> void Renderer_DestroyContext(const RenderContext<D> *context);
void Renderer_FlushAllContexts();
void Renderer_ReloadPipelines();

// TODO(Isma): Remove this. will not be necessary, onyx.hpp handles it
void Renderer_AddTarget(const ViewMask vmask);
void Renderer_RemoveTarget(const ViewMask vmask);

// TODO(Isma): Add a bit more clearance on what these do. They essentially write into the renderer's descriptor sets
template <Dimension D>
void Renderer_BindBuffer(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void Renderer_BindImage(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void Renderer_BindSampler(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement = 0);

Graph::Sampler Renderer_GetNearSampler();

template <Dimension D> const ten<VkDescriptorSet, Geometry_Count> &Renderer_GetDescriptorSets(RenderPass pass);

// consider having arrays of semaphores to allow for some flexibility

// NOTE(Isma): Same family index multi queue support was dropped. passing queues around is in theory no longer needed
// anymore
TransferSubmitInfo Renderer_Transfer(Graph::CommandBuffer command, u32 maxLights = 1024, u32 maxReleaseBarriers = 256);
void Renderer_SubmitTransfer(Graph::CommandPool pool, TKit::Span<const TransferSubmitInfo> info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
void Renderer_PrepareRender();
void Renderer_ApplyAcquireBarriers(Graph::CommandBuffer graphicsCommand);

RenderSubmitInfo Renderer_Render(Graph::CommandBuffer command, Window *window, RenderFlags flags = 0);
RenderSubmitInfo Renderer_Render(Graph::CommandBuffer command, RenderTexture *rtex);

void Renderer_SubmitRender(Graph::CommandPool pool, TKit::Span<const RenderSubmitInfo> info);
void Renderer_Coalesce(u32 maxRanges = 512);

} // namespace Onyx
