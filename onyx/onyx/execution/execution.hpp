#pragma once

#include "onyx/core/alias.hpp"
#include "vkit/execution/command_pool.hpp"
#include "vkit/execution/queue.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
struct CommandPool;
}

namespace Onyx::Execution
{
struct Specs
{
    u32 MaxCommandPools = 16;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

struct Tracker
{
    const VKit::Queue *Queue = nullptr;
    u64 InFlightValue = 0;

    bool InUse() const
    {
        return Queue && Queue->GetCompletedTimeline() < InFlightValue;
    }
    void MarkInUse(const VKit::Queue *queue, const u64 inFlightValue)
    {
        Queue = queue;
        InFlightValue = inFlightValue;
    }
};

ONYX_NO_DISCARD Result<> UpdateCompletedQueueTimelines();
void RevokeUnsubmittedQueueTimelines();

VKit::Queue *FindSuitableQueue(VKit::QueueType type);

ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(u32 family);
ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(VKit::QueueType type);

ONYX_NO_DISCARD Result<VkCommandBuffer> Allocate(CommandPool *pool);
void MarkInUse(CommandPool *pool, const VKit::Queue *queue, u64 inFlightValue);

ONYX_NO_DISCARD Result<> BeginCommandBuffer(VkCommandBuffer commandBuffer);
ONYX_NO_DISCARD Result<> EndCommandBuffer(VkCommandBuffer commandBuffer);

u32 GetFamilyIndex(VKit::QueueType type);

bool IsSeparateTransferMode();

VKit::CommandPool &GetTransientGraphicsPool();
VKit::CommandPool &GetTransientTransferPool();

struct SyncData
{
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
};

ONYX_NO_DISCARD Result<TKit::TierArray<SyncData>> CreateSyncData(u32 imageCount);
void DestroySyncData(TKit::Span<const SyncData> objects);

} // namespace Onyx::Execution
