#pragma once

#include "onyx/core.hpp"
#include "onyx/window.hpp"
#include "onyx/render_texture.hpp"
#include "onyx/instance.hpp"
#include "execution.hpp"
#include "pass.hpp"
#include "vkit/execution/queue.hpp"
#include "vkit/resource/sampler.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx
{
template <Dimension D> class RenderContext;

struct TransferSubmitInfo
{
    VkCommandBuffer Command = VK_NULL_HANDLE;
    u64 InFlightValue = 0;
    VkSemaphoreSubmitInfoKHR SignalSemaphore{};

    operator bool() const
    {
        return Command != VK_NULL_HANDLE;
    }
};

struct RenderSubmitInfo
{
    VkCommandBuffer Command = VK_NULL_HANDLE;
    u64 InFlightValue = 0;
    TKit::StaticArray<VkSemaphoreSubmitInfoKHR, 2> SignalSemaphores{};
    TKit::TierArray<VkSemaphoreSubmitInfoKHR> WaitSemaphores{};
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

VkPipelineRenderingCreateInfoKHR CreateGeometryPipelineRenderingCreateInfo();

template <Dimension D> RenderContext<D> *CreateContext(u32 immediateDynamicMeshCapacity);
template <Dimension D> void DestroyContext(RenderContext<D> *context);
void FlushAllContexts();
void ReloadPipelines();
bool IsDepthSupportedFor2D();

// TODO(Isma): Remove this. will not be necessary, onyx.hpp handles it
void AddTarget(const ViewMask vmask);
void RemoveTarget(const ViewMask vmask);

// TODO(Isma): Add a bit more clearance on what these do. They essentially write into the renderer's descriptor sets
template <Dimension D>
void BindBuffer(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void BindImage(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement = 0);

const VKit::Sampler &GetNearSampler();

template <Dimension D> const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(RenderPass pass);

// consider having arrays of semaphores to allow for some flexibility

// NOTE(Isma): Same family index multi queue support was dropped. passing queues around is in theory no longer needed
// anymore
TransferSubmitInfo Transfer(VKit::Queue *transfer, VkCommandBuffer command, u32 maxLights = 1024,
                            u32 maxReleaseBarriers = 256);
void SubmitTransfer(VKit::Queue *transfer, CommandPool *pool, TKit::Span<const TransferSubmitInfo> info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
void PrepareRender();
void ApplyAcquireBarriers(VkCommandBuffer graphicsCommand);

RenderSubmitInfo Render(VKit::Queue *graphics, VkCommandBuffer command, Window *window, RenderFlags flags = 0);
RenderSubmitInfo Render(VKit::Queue *graphics, VkCommandBuffer command, RenderTexture *rtex);

void SubmitRender(VKit::Queue *graphics, CommandPool *pool, TKit::Span<const RenderSubmitInfo> info);
void Coalesce(u32 maxRanges = 512);

// TODO(Isma): This right now is unused and inaccessible. Do something about this
#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void DisplayMemoryLayout();
#endif

} // namespace Onyx::Renderer
