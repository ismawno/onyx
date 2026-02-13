#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/execution/execution.hpp"
#include "vkit/execution/queue.hpp"

namespace Onyx
{
class Window;
template <Dimension D> class RenderContext;
} // namespace Onyx

namespace Onyx::Renderer
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

VkPipelineRenderingCreateInfoKHR CreatePipelineRenderingCreateInfo();

template <Dimension D> RenderContext<D> *CreateContext();
template <Dimension D> void DestroyContext(RenderContext<D> *context);

void ClearWindow(const Window *window);

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
    TKit::FixedArray<VkSemaphoreSubmitInfoKHR, 2> SignalSemaphores{};
    TKit::TierArray<VkSemaphoreSubmitInfoKHR> WaitSemaphores{};
};

ONYX_NO_DISCARD Result<TransferSubmitInfo> Transfer(VKit::Queue *transfer, VkCommandBuffer command);
ONYX_NO_DISCARD Result<> SubmitTransfer(VKit::Queue *transfer, CommandPool *pool,
                                        TKit::Span<const TransferSubmitInfo> info);

void ApplyAcquireBarriers(VkCommandBuffer graphicsCommand);

ONYX_NO_DISCARD Result<RenderSubmitInfo> Render(VKit::Queue *graphics, VkCommandBuffer command, Window *window);
ONYX_NO_DISCARD Result<> SubmitRender(VKit::Queue *graphics, CommandPool *pool,
                                      TKit::Span<const RenderSubmitInfo> info);

void Coalesce();

template <Dimension D> void BindStaticMeshes(VkCommandBuffer command);
template <Dimension D> void DrawStaticMesh(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);

} // namespace Onyx::Renderer
