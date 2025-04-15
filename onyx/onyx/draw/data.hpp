#pragma once

#include "onyx/draw/vertex.hpp"
#include "onyx/core/core.hpp"
#include "vkit/buffer/device_local_buffer.hpp"
#include "vkit/buffer/host_visible_buffer.hpp"
#include "tkit/container/buffer.hpp"
#include "tkit/container/span.hpp"

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

namespace Onyx
{
using Index = ONYX_INDEX_TYPE;

template <Dimension D> struct IndexVertexData
{
    TKit::DynamicArray<Vertex<D>> Vertices;
    TKit::DynamicArray<Index> Indices;
};

template <Dimension D> VKit::FormattedResult<IndexVertexData<D>> Load(std::string_view p_Path) noexcept;

template <Dimension D> using DeviceLocalVertexBuffer = VKit::DeviceLocalBuffer<Vertex<D>>;
using DeviceLocalIndexBuffer = VKit::DeviceLocalBuffer<Index>;
template <typename T> using DeviceLocalStorageBuffer = VKit::DeviceLocalBuffer<T>;

template <Dimension D> using HostVisibleVertexBuffer = VKit::HostVisibleBuffer<Vertex<D>>;
using HostVisibleIndexBuffer = VKit::HostVisibleBuffer<Index>;
template <typename T> using HostVisibleStorageBuffer = VKit::HostVisibleBuffer<T>;

template <Dimension D> using HostVertexBuffer = TKit::Buffer<Vertex<D>>;
using HostIndexBuffer = TKit::Buffer<Index>;
template <typename T> using HostStorageBuffer = TKit::Buffer<T>;

template <Dimension D>
DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(TKit::Span<const Vertex<D>> p_Vertices) noexcept;
ONYX_API DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(TKit::Span<const Index> p_Indices) noexcept;

template <typename T>
DeviceLocalStorageBuffer<T> CreateDeviceLocalStorageBuffer(const TKit::Span<const T> p_Data) noexcept
{
    typename VKit::DeviceLocalBuffer<T>::StorageSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Data;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();

    const VKit::PhysicalDevice &device = Core::GetDevice().GetPhysicalDevice();
    const VkDeviceSize alignment = device.GetInfo().Properties.Core.limits.minStorageBufferOffsetAlignment;
    const auto result = VKit::DeviceLocalBuffer<T>::CreateStorageBuffer(specs, alignment);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(VkDeviceSize p_Capacity) noexcept;
ONYX_API HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(VkDeviceSize p_Capacity) noexcept;

template <typename T> HostVisibleStorageBuffer<T> CreateHostVisibleStorageBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<T>::StorageSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;

    const VKit::PhysicalDevice &device = Core::GetDevice().GetPhysicalDevice();
    const VkDeviceSize alignment = device.GetInfo().Properties.Core.limits.minStorageBufferOffsetAlignment;
    const auto result = VKit::HostVisibleBuffer<T>::CreateStorageBuffer(specs, alignment);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <typename T> HostStorageBuffer<T> CreateHostStorageBuffer(const u32 p_Capacity) noexcept
{
    const VKit::PhysicalDevice &device = Core::GetDevice().GetPhysicalDevice();
    const HostStorageBuffer<T> buffer{p_Capacity,
                                      device.GetInfo().Properties.Core.limits.minStorageBufferOffsetAlignment};
    return buffer;
}

} // namespace Onyx