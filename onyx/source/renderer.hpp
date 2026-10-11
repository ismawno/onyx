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

// honestly, apart from imgui, rest can be toggled per view. these may be a bit redundant
using RenderFlags = u8;
enum RenderFlagBit : RenderFlags
{
#ifdef ONYX_ENABLE_IMGUI
    RenderFlag_ImGui = 1U << 0,
#endif
};

} // namespace Onyx

namespace Onyx::Renderer
{

void Initialize(const Specs &specs);
void Terminate();

template <Dimension D> RenderContext<D> *CreateContext(u32 immediateDynamicMeshCapacity);
template <Dimension D> void DestroyContext(const RenderContext<D> *context);
void FlushAllContexts();
void ReloadPipelines();
bool IsDepthSupportedFor2D();

// TODO(Isma): Remove this. will not be necessary, onyx.hpp handles it
void AddTarget(const ViewMask vmask);
void RemoveTarget(const ViewMask vmask);

// TODO(Isma): Add a bit more clearance on what these do. They essentially write into the renderer's descriptor sets
template <Dimension D>
void BindBuffer(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void BindImage(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass, u32 dstElement = 0);
template <Dimension D> void BindSampler(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement = 0);

Graph::Sampler GetNearSampler();

template <Dimension D> const ten<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(RenderPass pass);

// consider having arrays of semaphores to allow for some flexibility

// NOTE(Isma): Same family index multi queue support was dropped. passing queues around is in theory no longer needed
// anymore
TransferSubmitInfo Transfer(Graph::CommandBuffer command, u32 maxLights = 1024, u32 maxReleaseBarriers = 256);
void SubmitTransfer(Graph::CommandPool pool, TKit::Span<const TransferSubmitInfo> info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
void PrepareRender();
void ApplyAcquireBarriers(Graph::CommandBuffer graphicsCommand);

RenderSubmitInfo Render(Graph::CommandBuffer command, Window *window, RenderFlags flags = 0);
RenderSubmitInfo Render(Graph::CommandBuffer command, RenderTexture *rtex);

void SubmitRender(Graph::CommandPool pool, TKit::Span<const RenderSubmitInfo> info);
void Coalesce(u32 maxRanges = 512);

} // namespace Onyx::Renderer
