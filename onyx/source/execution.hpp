#pragma once

#include "onyx/alias.hpp"
#include "onyx/core.hpp"
#include "vkit/execution/command_pool.hpp"
#include "vkit/execution/queue.hpp"

namespace Onyx
{
// TODO(Isma): Unhide this? completely unnecessary
struct CommandPool;
} // namespace Onyx

namespace Onyx::Execution
{
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

VKit::Queue *GetQueue(VKit::QueueType type);
u32 GetFamilyIndex(VKit::QueueType type);

CommandPool *FindAvailableCommandPool(u32 family);
inline CommandPool *FindAvailableCommandPool(const VKit::QueueType type)
{
    return FindAvailableCommandPool(GetFamilyIndex(type));
}

VkCommandBuffer Allocate(CommandPool *pool);
void MarkInUse(CommandPool *pool, const VKit::Queue *queue, u64 inFlightValue);

void BeginCommandBuffer(VkCommandBuffer commandBuffer);
void EndCommandBuffer(VkCommandBuffer commandBuffer);

bool IsSeparateTransferMode();

VKit::CommandPool &GetTransientGraphicsPool();
VKit::CommandPool &GetTransientTransferPool();
} // namespace Onyx::Execution
