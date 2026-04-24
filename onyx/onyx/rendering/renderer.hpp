#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/rendering/view.hpp"
#include "onyx/rendering/pass.hpp"
#include "vkit/execution/queue.hpp"
#include "vkit/resource/sampler.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx
{
template <Dimension D> class RenderContext;
struct RenderTargetInfo
{
    TKit::TierArray<RenderView<D2> *> Views2;
    TKit::TierArray<RenderView<D3> *> Views3;
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
};

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
    RenderFlag_ImGui = 1 << 0,
};

} // namespace Onyx

namespace Onyx::Renderer
{
template <Dimension D> struct ShadowSpecs;
// TODO(Isma): these names should change...
template <> struct ShadowSpecs<D2>
{
    VkFormat OcclusionFormat = VK_FORMAT_R8_UNORM;
    VkFormat ShadowFormat = VK_FORMAT_R16_SFLOAT;
    u32 OcclusionResolution = 1024;
    u32 ShadowResolution = 1024;
};
template <> struct ShadowSpecs<D3>
{
    VkFormat ShadowFormat = VK_FORMAT_D32_SFLOAT;
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{1024, 2048};
};

struct Specs
{
    ShadowSpecs<D2> Shadows2{};
    ShadowSpecs<D3> Shadows3{};
};

void Initialize(const Specs &specs);
void Terminate();

VkPipelineRenderingCreateInfoKHR CreateGeometryPipelineRenderingCreateInfo();

template <Dimension D> RenderContext<D> *CreateContext();
template <Dimension D> void DestroyContext(RenderContext<D> *context);
void FlushAllContexts();
void ReloadPipelines();

void AddTarget(const ViewMask vmask);
void RemoveTarget(const ViewMask vmask);

// TODO(Isma): Add a bit more clearance on what these do. They essentially write into the renderer's descriptor sets
template <Dimension D>
void BindBuffer(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void BindImage(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement = 0);

const VKit::Sampler &GetGeneralPurposeSampler();

template <Dimension D> const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(RenderPass pass);

// consider having arrays of semaphores to allow for some flexibility

TransferSubmitInfo Transfer(VKit::Queue *transfer, VkCommandBuffer command, u32 maxReleaseBarriers = 256);
void SubmitTransfer(VKit::Queue *transfer, CommandPool *pool, TKit::Span<const TransferSubmitInfo> info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
void PrepareRender();
void ApplyAcquireBarriers(VkCommandBuffer graphicsCommand);

RenderSubmitInfo Render(VKit::Queue *graphics, VkCommandBuffer command, Window *window, RenderFlags flags = 0);

void SubmitRender(VKit::Queue *graphics, CommandPool *pool, TKit::Span<const RenderSubmitInfo> info);

void Coalesce(u32 maxRanges = 512);

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void DisplayMemoryLayout();
#endif

} // namespace Onyx::Renderer
