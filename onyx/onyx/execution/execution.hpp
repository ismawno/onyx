#pragma once

#include "onyx/execution/command_pool.hpp"

namespace Onyx::Execution
{
struct Specs
{
    u32 MaxCommandPools = 16;
};
void Initialize(const Specs &p_Specs);
void Terminate();

void UpdateCompletedQueueTimelines();
void RevokeUnsubmittedQueueTimelines();

VKit::Queue *FindSuitableQueue(VKit::QueueType p_Type);

CommandPool &FindSuitableCommandPool(u32 p_Family);
CommandPool &FindSuitableCommandPool(VKit::QueueType p_Type);

void BeginCommandBuffer(VkCommandBuffer p_CommandBuffer);
void EndCommandBuffer(VkCommandBuffer p_CommandBuffer);

u32 GetFamilyIndex(VKit::QueueType p_Type);

bool IsSeparateTransferMode();

VKit::CommandPool &GetTransientGraphicsPool();
VKit::CommandPool &GetTransientTransferPool();

struct SyncData
{
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
};

TKit::TierArray<SyncData> CreateSyncData(u32 p_ImageCount);
void DestroySyncData(TKit::Span<const SyncData> p_Objects);

} // namespace Onyx::Execution
