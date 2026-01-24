#pragma once

#include "onyx/execution/command_pool.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Execution
{
struct Specs
{
    u32 MaxCommandPools = 16;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &p_Specs);
void Terminate();

ONYX_NO_DISCARD Result<> UpdateCompletedQueueTimelines();
void RevokeUnsubmittedQueueTimelines();

VKit::Queue *FindSuitableQueue(VKit::QueueType p_Type);

ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(u32 p_Family);
ONYX_NO_DISCARD Result<CommandPool *> FindSuitableCommandPool(VKit::QueueType p_Type);

ONYX_NO_DISCARD Result<> BeginCommandBuffer(VkCommandBuffer p_CommandBuffer);
ONYX_NO_DISCARD Result<> EndCommandBuffer(VkCommandBuffer p_CommandBuffer);

u32 GetFamilyIndex(VKit::QueueType p_Type);

bool IsSeparateTransferMode();

VKit::CommandPool &GetTransientGraphicsPool();
VKit::CommandPool &GetTransientTransferPool();

struct SyncData
{
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
};

ONYX_NO_DISCARD Result<TKit::TierArray<SyncData>> CreateSyncData(u32 p_ImageCount);
void DestroySyncData(TKit::Span<const SyncData> p_Objects);

} // namespace Onyx::Execution
