#pragma once

#include "onyx/core/core.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Resources
{
ONYX_NO_DISCARD Result<VKit::DeviceBuffer> CreateBuffer(VKit::DeviceBufferFlags flags, VkDeviceSize size);

template <typename T>
ONYX_NO_DISCARD Result<VKit::DeviceBuffer> CreateBuffer(const VKit::DeviceBufferFlags flags,
                                                        const u32 capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    return CreateBuffer(flags, capacity * sizeof(T));
}

inline u32 GrowCapacity(const u32 instances, const f32 factor = 1.5f)
{
    return u32(factor * f32(instances));
}

Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, u32 instances, u32 instanceSize, const f32 factor = 1.5f);

template <typename T>
Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const u32 instances, const f32 factor = 1.5f)
{
    return GrowBufferIfNeeded(buffer, instances, sizeof(T), factor);
}

} // namespace Onyx::Resources
