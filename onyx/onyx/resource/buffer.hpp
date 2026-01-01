#pragma once

#include "onyx/core/core.hpp"
#include "vkit/resource/buffer.hpp"
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
enum BufferFlags : VKit::BufferFlags
{
    Buffer_DeviceVertex = VKit::BufferFlag_Vertex | VKit::BufferFlag_DeviceLocal,
    Buffer_DeviceIndex = VKit::BufferFlag_Index | VKit::BufferFlag_DeviceLocal,
    Buffer_DeviceStorage = VKit::BufferFlag_Storage | VKit::BufferFlag_DeviceLocal,
    Buffer_Staging = VKit::BufferFlag_Staging | VKit::BufferFlag_HostMapped,
};

using Index = ONYX_INDEX_TYPE;

using DeviceLocalBuffer = VKit::Buffer;
using HostVisibleBuffer = VKit::Buffer;

template <typename T> using HostBuffer = TKit::DynamicArray<T>;

template <typename T>
VKit::Buffer CreateBuffer(const VKit::BufferFlags p_Flags, const u32 p_Capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const auto result =
        VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags).SetSize<T>(p_Capacity).Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <typename T> VKit::Buffer CreateBuffer(const VKit::BufferFlags p_Flags, const HostBuffer<T> &p_Data)
{
    auto result = VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
                      .SetSize<T>(p_Data.GetSize())
                      .Build();
    VKIT_ASSERT_RESULT(result);
    VKit::Buffer &buffer = result.GetValue();
    if (buffer.GetInfo().Flags & VKit::BufferFlag_HostVisible)
        buffer.Write<T>(p_Data);
    else
    {
        QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);
        VKIT_ASSERT_EXPRESSION(buffer.UploadFromHost<T>(Core::GetTransferPool(), queue->Queue, p_Data));
        Core::ReturnQueue(queue);
    }
    return buffer;
}

template <typename T>
bool GrowBufferIfNeeded(VKit::Buffer &p_Buffer, const u32 p_Instances, const VKit::BufferFlags p_Flags,
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
