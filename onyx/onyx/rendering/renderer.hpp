#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/property/camera.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/rendering/pass.hpp"
#include "vkit/execution/queue.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx
{
template <Dimension D> class RenderContext;
struct ViewInfo
{
    TKit::TierArray<CameraInfo<D2>> Cameras2;
    TKit::TierArray<CameraInfo<D3>> Cameras3;
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
    ViewMask ViewBit;

    template <Dimension D> const auto &GetCameraInfos() const
    {
        if constexpr (D == D2)
            return Cameras2;
        else
            return Cameras3;
    }
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
    u32 OcclusionResolution = 256;
    u32 ShadowResolution = 256;
};
template <> struct ShadowSpecs<D3>
{
    VkFormat ShadowFormat = VK_FORMAT_D32_SFLOAT;
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{512, 1024};
};

struct Specs
{
    ShadowSpecs<D2> Shadows2{};
    ShadowSpecs<D3> Shadows3{};
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

VkPipelineRenderingCreateInfoKHR CreateGeometryPipelineRenderingCreateInfo();

template <Dimension D> ONYX_NO_DISCARD Result<RenderContext<D> *> CreateContext();
template <Dimension D> void DestroyContext(RenderContext<D> *context);
void FlushAllContexts();
ONYX_NO_DISCARD Result<> ReloadPipelines();

void AddTarget(const ViewMask vmask);
void RemoveTarget(const ViewMask vmask);

template <Dimension D>
void WriteBuffer(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement = 0);
template <Dimension D>
void WriteImage(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement = 0);

template <Dimension D> const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(RenderPass pass);

// consider having arrays of semaphores to allow for some flexibility

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

ONYX_NO_DISCARD Result<TransferSubmitInfo> Transfer(VKit::Queue *transfer, VkCommandBuffer command,
                                                    u32 maxReleaseBarriers = 256);
ONYX_NO_DISCARD Result<> SubmitTransfer(VKit::Queue *transfer, CommandPool *pool,
                                        TKit::Span<const TransferSubmitInfo> info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
void PrepareRender();
void ApplyAcquireBarriers(VkCommandBuffer graphicsCommand);

ONYX_NO_DISCARD Result<> RenderShadows(VKit::Queue *graphics, VkCommandBuffer command, ViewMask viewBit);
ONYX_NO_DISCARD Result<RenderSubmitInfo> RenderGeometry(VKit::Queue *graphics, VkCommandBuffer command,
                                                        const ViewInfo &vinfo);
ONYX_NO_DISCARD Result<> SubmitRender(VKit::Queue *graphics, CommandPool *pool,
                                      TKit::Span<const RenderSubmitInfo> info);

void Coalesce(u32 maxRanges = 512);

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D> void DisplayMemoryLayout();
#endif

} // namespace Onyx::Renderer
