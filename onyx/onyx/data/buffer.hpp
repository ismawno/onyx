#pragma once

#include "onyx/property/vertex.hpp"
#include "onyx/core/core.hpp"
#include "vkit/resource/buffer.hpp"
#include "tkit/container/dynamic_array.hpp"

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

namespace Onyx
{
namespace Detail
{
// These change behaviour depending if the physical device has a transfer queue different to the graphics queue used.

VkBufferMemoryBarrier CreateAcquireBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size,
                                           VkAccessFlags p_DstFlags = VK_ACCESS_SHADER_READ_BIT);
VkBufferMemoryBarrier CreateReleaseBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size);

void ApplyAcquireBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers,
                         VkPipelineStageFlags p_DstFlags);
void ApplyReleaseBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers);
} // namespace Detail

using Index = ONYX_INDEX_TYPE;

using DeviceLocalBuffer = VKit::Buffer;
using HostVisibleBuffer = VKit::Buffer;

template <typename T> using HostBuffer = TKit::DynamicArray<T>;
template <Dimension D> using HostVertexBuffer = TKit::DynamicArray<Vertex<D>>;
using HostIndexBuffer = TKit::DynamicArray<Index>;

template <Dimension D> struct IndexVertexHostData
{
    HostVertexBuffer<D> Vertices;
    HostIndexBuffer Indices;
};

#ifdef ONYX_ENABLE_OBJ
template <Dimension D>
VKit::Result<IndexVertexHostData<D>> Load(std::string_view p_Path, const f32m<D> *p_Transform = nullptr);
#endif

template <typename T> VKit::Buffer CreateBuffer(const VKit::Buffer::Flags p_Flags, const u32 p_Capacity)
{
    const auto result =
        VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags).SetSize<T>(p_Capacity).Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <typename T> VKit::Buffer CreateBuffer(const VKit::Buffer::Flags p_Flags, const HostBuffer<T> &p_Data)
{
    auto result = VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
                      .SetSize<T>(p_Data.GetSize())
                      .Build();
    VKIT_ASSERT_RESULT(result);
    VKit::Buffer &buffer = result.GetValue();
    if (buffer.GetInfo().Flags & VKit::Buffer::Flag_HostVisible)
        buffer.Write<T>(p_Data);
    else
    {
        QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);
        VKIT_ASSERT_EXPRESSION(buffer.UploadFromHost<T>(Core::GetTransferPool(), queue->Queue, p_Data));
        Core::ReturnQueue(queue);
    }
    return buffer;
}

} // namespace Onyx
