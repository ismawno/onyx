#pragma once

#include "onyx/property/vertex.hpp"
#include "onyx/core/core.hpp"
#include "vkit/buffer/device_local_buffer.hpp"
#include "vkit/buffer/host_visible_buffer.hpp"
#include "tkit/container/dynamic_array.hpp"

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

namespace Onyx
{
namespace Detail
{

// These change behaviour depending if the physical device has a transfer queue different to the graphics queue used.

void RecordCopy(VkCommandBuffer p_CommandBuffer, VkBuffer p_DeviceLocalBuffer, VkBuffer p_DeviceStagingBuffer,
                u32 p_Size) noexcept;

VkBufferMemoryBarrier CreateAcquireBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size,
                                           VkAccessFlags p_DstFlags = VK_ACCESS_SHADER_READ_BIT) noexcept;
VkBufferMemoryBarrier CreateReleaseBarrier(VkBuffer p_DeviceLocalBuffer, u32 p_Size) noexcept;

void ApplyAcquireBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers) noexcept;
void ApplyReleaseBarrier(VkCommandBuffer p_CommandBuffer, TKit::Span<const VkBufferMemoryBarrier> p_Barriers) noexcept;
} // namespace Detail

using Index = ONYX_INDEX_TYPE;

template <Dimension D> using DeviceLocalVertexBuffer = VKit::DeviceLocalBuffer<Vertex<D>>;
using DeviceLocalIndexBuffer = VKit::DeviceLocalBuffer<Index>;
template <typename T> using DeviceLocalStorageBuffer = VKit::DeviceLocalBuffer<T>;

template <Dimension D> using HostVisibleVertexBuffer = VKit::HostVisibleBuffer<Vertex<D>>;
using HostVisibleIndexBuffer = VKit::HostVisibleBuffer<Index>;
template <typename T> using HostVisibleStorageBuffer = VKit::HostVisibleBuffer<T>;

template <Dimension D> using HostVertexBuffer = TKit::DynamicArray<Vertex<D>>;
using HostIndexBuffer = TKit::DynamicArray<Index>;
template <typename T> using HostStorageBuffer = TKit::DynamicArray<T>;

template <Dimension D> struct IndexVertexHostData
{
    HostVertexBuffer<D> Vertices;
    HostIndexBuffer Indices;
};

template <Dimension D>
VKit::FormattedResult<IndexVertexHostData<D>> Load(std::string_view p_Path,
                                                   const fmat<D> *p_Transform = nullptr) noexcept;

template <Dimension D>
DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(const HostVertexBuffer<D> &p_Vertices) noexcept;
ONYX_API DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(const HostIndexBuffer &p_Indices) noexcept;

template <Dimension D> DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(u32 p_Capacity) noexcept;
ONYX_API DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(u32 p_Capacity) noexcept;

template <typename T> DeviceLocalStorageBuffer<T> CreateDeviceLocalStorageBuffer(const u32 p_Capacity) noexcept
{
    typename VKit::DeviceLocalBuffer<T>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = TKit::Span<const T>{nullptr, p_Capacity};
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();

    const auto result = VKit::DeviceLocalBuffer<T>::CreateStorageBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(u32 p_Capacity) noexcept;
ONYX_API HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(u32 p_Capacity) noexcept;

template <typename T> HostVisibleStorageBuffer<T> CreateHostVisibleStorageBuffer(const u32 p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<T>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    specs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    const auto result = VKit::HostVisibleBuffer<T>::CreateStorageBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

} // namespace Onyx
