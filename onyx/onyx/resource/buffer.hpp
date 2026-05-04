#pragma once

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 16
#endif

#include "vkit/resource/device_buffer.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
using enum VKit::DeviceBufferFlagBit;

enum BufferType : VKit::DeviceBufferFlags
{
    Buffer_DeviceVertex = DeviceBufferFlag_Vertex | DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceIndex = DeviceBufferFlag_Index | DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceStorage = DeviceBufferFlag_Storage | DeviceBufferFlag_DeviceLocal,
    Buffer_Staging = DeviceBufferFlag_Staging | DeviceBufferFlag_HostMapped | DeviceBufferFlag_HostRandomAccess,
    Buffer_HostVertex = DeviceBufferFlag_Vertex | DeviceBufferFlag_HostMapped | DeviceBufferFlag_HostRandomAccess,
    Buffer_HostIndex = DeviceBufferFlag_Index | DeviceBufferFlag_HostMapped | DeviceBufferFlag_HostRandomAccess,
};

inline VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags flags, const VkDeviceSize size)
{
    return ONYX_CHECK_EXPRESSION(
        VKit::DeviceBuffer::Builder(GetDevice(), GetVulkanAllocator(), flags).SetSize(size).Build());
}
template <typename T>
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags flags, const u32 capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    return CreateBuffer(flags, capacity * sizeof(T));
}

inline u32 GrowCapacity(const u32 instances, const f32 factor = 1.5f)
{
    return u32(factor * f32(instances));
}

inline bool GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const u32 instances, const u32 instanceSize,
                               const f32 factor = 1.5f)
{
    const VKit::DeviceBuffer::Info &info = buffer.GetInfo();
    const u32 inst = u32(info.Size / instanceSize);
    if (buffer && inst >= instances)
        return false;

    const u32 ninst = GrowCapacity(instances, factor);
    buffer.Destroy();
    buffer = Onyx::CreateBuffer(info.Flags, ninst * instanceSize);
    return true;
}

template <typename T> bool GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const u32 instances, const f32 factor = 1.5f)
{
    return GrowBufferIfNeeded(buffer, instances, sizeof(T), factor);
}

using Index = ONYX_INDEX_TYPE;
} // namespace Onyx
