#pragma once

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 16
#endif

#include "graph/resources.hpp"

namespace Onyx
{
using enum Graph::BufferFlagBit;

enum BufferType : Graph::BufferFlags
{
    Buffer_DeviceVertex = Graph::BufferFlag_Vertex | Graph::BufferFlag_DeviceLocal,
    Buffer_DeviceIndex = Graph::BufferFlag_Index | Graph::BufferFlag_DeviceLocal,
    Buffer_DeviceStorage = Graph::BufferFlag_Storage | Graph::BufferFlag_DeviceLocal,
    Buffer_Staging = Graph::BufferFlag_Staging | Graph::BufferFlag_HostMapped | Graph::BufferFlag_HostRandomAccess,
    Buffer_HostVertex = Graph::BufferFlag_Vertex | Graph::BufferFlag_HostMapped | Graph::BufferFlag_HostRandomAccess,
    Buffer_HostIndex = Graph::BufferFlag_Index | Graph::BufferFlag_HostMapped | Graph::BufferFlag_HostRandomAccess,
};

inline u32 GrowCapacity(const u32 instances, const f32 factor = 1.5f)
{
    return u32(factor * f32(instances));
}

inline bool GrowBufferIfNeeded(Graph::Buffer &buffer, const u32 instances, const u32 instanceSize,
                               const Graph::BufferFlags flags, const f32 factor = 1.5f)
{
    if (buffer != Graph::NullHandle)
    {
        const usz size = Graph::Buffer_GetSize(buffer);
        const u32 inst = u32(size / instanceSize);
        if (inst >= instances)
            return false;
    }

    const u32 ninst = GrowCapacity(instances, factor);
    if (buffer != Graph::NullHandle)
        Graph::Buffer_Destroy(buffer);
    buffer = Graph::Buffer_Create(ninst * instanceSize, flags);
    return true;
}

template <typename T>
bool GrowBufferIfNeeded(Graph::Buffer &buffer, const u32 instances, const Graph::BufferFlags flags,
                        const f32 factor = 1.5f)
{
    return GrowBufferIfNeeded(buffer, instances, sizeof(T), flags, factor);
}

} // namespace Onyx
