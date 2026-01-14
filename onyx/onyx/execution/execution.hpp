#pragma once

#include "onyx/execution/command_pool.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Execution
{
void Initialize();
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

PerImageData<SyncData> CreateSyncData(u32 p_ImageCount);
void DestroySyncData(TKit::Span<const SyncData> p_Objects);

} // namespace Onyx::Execution
