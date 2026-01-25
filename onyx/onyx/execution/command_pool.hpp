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
    u32 Family;
    VKit::CommandPool Pool{};
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
} // namespace Onyx
