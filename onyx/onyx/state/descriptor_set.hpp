#pragma once

#include "vkit/state/descriptor_set.hpp"

namespace Onyx
{
struct DescriptorSet
{
    const VKit::Queue *Queue = nullptr;
    u64 InFlightValue = 0;
    VkDescriptorSet Set = VK_NULL_HANDLE;
    VkBuffer Buffer = VK_NULL_HANDLE;

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
