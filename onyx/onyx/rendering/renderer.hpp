#pragma once

#include "onyx/core/core.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/execution/command_pool.hpp"
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
template <Dimension D> void DestroyContext(RenderContext<D> *p_Context);

void ClearWindow(const Window *p_Window);

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

ONYX_NO_DISCARD Result<TransferSubmitInfo> Transfer(VKit::Queue *p_Transfer, VkCommandBuffer p_Command);
ONYX_NO_DISCARD Result<> SubmitTransfer(VKit::Queue *p_Transfer, CommandPool *p_Pool,
                                        TKit::Span<const TransferSubmitInfo> p_Info);

void ApplyAcquireBarriers(VkCommandBuffer p_GraphicsCommand);

ONYX_NO_DISCARD Result<RenderSubmitInfo> Render(VKit::Queue *p_Graphics, VkCommandBuffer p_Command,
                                                const Window *p_Window);
ONYX_NO_DISCARD Result<> SubmitRender(VKit::Queue *p_Graphics, CommandPool *p_Pool,
                                      TKit::Span<const RenderSubmitInfo> p_Info);

void Coalesce();

template <Dimension D> void BindStaticMeshes(VkCommandBuffer p_Command);
template <Dimension D>
void DrawStaticMesh(VkCommandBuffer p_Command, Mesh p_Mesh, u32 p_FirstInstance, u32 p_InstanceCount);

} // namespace Onyx::Renderer
