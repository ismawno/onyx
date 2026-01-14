#pragma once

#include "onyx/core/alias.hpp"
#include "vkit/execution/command_pool.hpp"
#include "vkit/execution/queue.hpp"

namespace Onyx
{
struct CommandPool
{
    const VKit::Queue *Queue = nullptr;
    u64 InFlightValue = 0;
    VKit::CommandPool Pool{};
    bool InUse() const
    {
        return Queue && Queue->GetCompletedTimeline() < InFlightValue;
    }
    void MarkInUse(const VKit::Queue *p_Queue, const u64 p_InFlightValue)
    {
        Queue = p_Queue;
        InFlightValue = p_InFlightValue;
    }

    VkCommandBuffer Allocate();
    void Reset();
};
} // namespace Onyx
