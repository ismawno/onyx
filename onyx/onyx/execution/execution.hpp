#pragma once

#include "onyx/execution/command_pool.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Execution
{
struct Specs
{
    u32 MaxCommandPools = 16;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

ONYX_NO_DISCARD Result<> UpdateCompletedQueueTimelines();
void RevokeUnsubmittedQueueTimelines();

VKit::Queue *FindSuitableQueue(VKit::QueueType type);

ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(u32 family);
ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(VKit::QueueType type);

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
