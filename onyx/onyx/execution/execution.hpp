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
    u32 MaxCommandPools = 1024;
};
void Initialize(const Specs &specs);
void Terminate();

struct Tracker
{
    const VKit::Queue *Queue = nullptr;
    u64 InFlightValue = 0;

    bool InUse() const
    {
        return Queue && Queue->GetCompletedTimeline() < InFlightValue;
    }
    bool Submitted() const
    {
        return Queue && Queue->GetTimelineSubmissions() >= InFlightValue;
    }
    bool InFlight() const
    {
        return Submitted() && InUse();
    }
    void MarkInUse(const VKit::Queue *queue, const u64 inFlightValue)
    {
        Queue = queue;
        InFlightValue = inFlightValue;
    }
};

void UpdateCompletedQueueTimelines();

VKit::Queue *FindSuitableQueue(VKit::QueueType type);

CommandPool * FindSuitableCommandPool(u32 family);
CommandPool * FindSuitableCommandPool(VKit::QueueType type);

VkCommandBuffer Allocate(CommandPool *pool);
void MarkInUse(CommandPool *pool, const VKit::Queue *queue, u64 inFlightValue);

void BeginCommandBuffer(VkCommandBuffer commandBuffer);
void EndCommandBuffer(VkCommandBuffer commandBuffer);

u32 GetFamilyIndex(VKit::QueueType type);

bool IsSeparateTransferMode();

VKit::CommandPool &GetTransientGraphicsPool();
VKit::CommandPool &GetTransientTransferPool();
} // namespace Onyx::Execution
