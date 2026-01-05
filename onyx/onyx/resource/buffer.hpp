#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/queues.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "tkit/container/dynamic_array.hpp"

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

namespace Onyx::Detail
{
// These change behaviour depending if the physical device has a transfer queue different to the graphics queue used.

VkBufferMemoryBarrier CreateAcquireBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size,
                                           VkAccessFlags p_DstFlags = VK_ACCESS_SHADER_READ_BIT);
VkBufferMemoryBarrier CreateReleaseBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size);

void ApplyAcquireBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers,
                         VkPipelineStageFlags p_DstFlags);
void ApplyReleaseBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers);
} // namespace Onyx::Detail

namespace Onyx
{
enum DeviceBufferFlags : VKit::DeviceBufferFlags
{
    Buffer_DeviceVertex = VKit::DeviceBufferFlag_Vertex | VKit::DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceIndex = VKit::DeviceBufferFlag_Index | VKit::DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceStorage = VKit::DeviceBufferFlag_Storage | VKit::DeviceBufferFlag_DeviceLocal,
    Buffer_Staging = VKit::DeviceBufferFlag_Staging | VKit::DeviceBufferFlag_HostMapped,
};

using Index = ONYX_INDEX_TYPE;

using DeviceLocalBuffer = VKit::DeviceBuffer;
using HostVisibleBuffer = VKit::DeviceBuffer;

template <typename T>
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags p_Flags,
                                const u32 p_Capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const auto result = VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
                            .SetSize<T>(p_Capacity)
                            .Build();
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}

template <typename T>
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags p_Flags, const TKit::DynamicArray<T> &p_Data)
{
    auto result = VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
                      .SetSize<T>(p_Data.GetSize())
                      .Build();
    VKIT_CHECK_RESULT(result);
    VKit::DeviceBuffer &buffer = result.GetValue();
    if (buffer.GetInfo().Flags & VKit::DeviceBufferFlag_HostVisible)
        buffer.Write<T>(p_Data);
    else
        VKIT_CHECK_EXPRESSION(
            buffer.UploadFromHost<T>(Queues::GetTransferPool(), *Queues::GetQueue(VKit::Queue_Transfer), p_Data));
    return buffer;
}

template <typename T>
bool GrowBufferIfNeeded(VKit::DeviceBuffer &p_Buffer, const u32 p_Instances, const VKit::DeviceBufferFlags p_Flags,
                        const f32 p_Factor = 1.5f)
{
    const u32 inst = p_Buffer.GetInfo().InstanceCount;
    if (p_Buffer && p_Instances <= inst)
        return false;

    const u32 ninst = static_cast<u32>(p_Factor * static_cast<f32>(p_Instances));
    p_Buffer.Destroy();
    p_Buffer = CreateBuffer<T>(p_Flags, ninst);
    return true;
}

} // namespace Onyx
