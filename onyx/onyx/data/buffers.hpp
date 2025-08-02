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

template <typename T>
DeviceLocalStorageBuffer<T> CreateDeviceLocalStorageBuffer(const HostStorageBuffer<T> &p_Data) noexcept
{
    typename VKit::DeviceLocalBuffer<T>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Data;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();

    const auto result = VKit::DeviceLocalBuffer<T>::CreateStorageBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(VkDeviceSize p_Capacity) noexcept;
ONYX_API HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(VkDeviceSize p_Capacity) noexcept;

template <typename T> HostVisibleStorageBuffer<T> CreateHostVisibleStorageBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<T>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    const auto result = VKit::HostVisibleBuffer<T>::CreateStorageBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

} // namespace Onyx
