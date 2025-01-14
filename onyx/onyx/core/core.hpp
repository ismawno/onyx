#pragma once

#include "onyx/draw/data.hpp"
#include "onyx/core/vma.hpp"
#include "vkit/backend/physical_device.hpp"
#include "vkit/descriptors/descriptor_pool.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"
#include "vkit/backend/command_pool.hpp"
#include "tkit/profiling/vulkan.hpp"
#include "tkit/container/span.hpp"

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 1024
#endif

#ifndef ONYX_MAX_DESCRIPTORS
#    define ONYX_MAX_DESCRIPTORS 1024
#endif

#ifndef ONYX_MAX_FRAMES_IN_FLIGHT
#    define ONYX_MAX_FRAMES_IN_FLIGHT 2
#endif

namespace TKit
{
class StackAllocator;
class ITaskManager;
} // namespace TKit

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
template <typename T> using PerFrameData = TKit::Array<T, ONYX_MAX_FRAMES_IN_FLIGHT>;
struct ONYX_API Core
{
    static void Initialize(TKit::ITaskManager *p_TaskManager) noexcept;
    static void Terminate() noexcept;

    static TKit::ITaskManager *GetTaskManager() noexcept;

    static const VKit::Instance &GetInstance() noexcept;
    static const VKit::LogicalDevice &GetDevice() noexcept;
    static void CreateDevice(VkSurfaceKHR p_Surface) noexcept;

    static bool IsDeviceCreated() noexcept;
    static void DeviceWaitIdle() noexcept;

    static VKit::CommandPool &GetCommandPool() noexcept;
    static VmaAllocator GetVulkanAllocator() noexcept;

    static VKit::DeletionQueue &GetDeletionQueue() noexcept;

    static const VKit::DescriptorPool &GetDescriptorPool() noexcept;
    static const VKit::DescriptorSetLayout &GetTransformStorageDescriptorSetLayout() noexcept;
    static const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout() noexcept;

    template <Dimension D> static VertexBuffer<D> CreateVertexBuffer(TKit::Span<const Vertex<D>> p_Vertices) noexcept;
    static IndexBuffer CreateIndexBuffer(TKit::Span<const Index> p_Indices) noexcept;
    template <typename T> static StorageBuffer<T> CreateStorageBuffer(const TKit::Span<const T> p_Data) noexcept
    {
        typename VKit::DeviceLocalBuffer<T>::StorageSpecs specs{};
        specs.Allocator = GetVulkanAllocator();
        specs.Data = p_Data;
        specs.CommandPool = &GetCommandPool();
        specs.Queue = GetGraphicsQueue();

        const VKit::PhysicalDevice &device = GetDevice().GetPhysicalDevice();
        const VkDeviceSize alignment = device.GetInfo().Properties.Core.limits.minStorageBufferOffsetAlignment;
        const auto result = VKit::DeviceLocalBuffer<T>::CreateStorageBuffer(specs, alignment);
        VKIT_ASSERT_RESULT(result);
        return result.GetValue();
    }

    template <Dimension D> static MutableVertexBuffer<D> CreateMutableVertexBuffer(VkDeviceSize p_Capacity) noexcept;
    static MutableIndexBuffer CreateMutableIndexBuffer(VkDeviceSize p_Capacity) noexcept;
    template <typename T>
    static MutableStorageBuffer<T> CreateMutableStorageBuffer(const VkDeviceSize p_Capacity) noexcept
    {
        typename VKit::HostVisibleBuffer<T>::StorageSpecs specs{};
        specs.Allocator = GetVulkanAllocator();
        specs.Capacity = p_Capacity;

        const VKit::PhysicalDevice &device = GetDevice().GetPhysicalDevice();
        const VkDeviceSize alignment = device.GetInfo().Properties.Core.limits.minStorageBufferOffsetAlignment;
        const auto result = VKit::HostVisibleBuffer<T>::CreateStorageBuffer(specs, alignment);
        VKIT_ASSERT_RESULT(result);
        return result.GetValue();
    }

    static VkQueue GetGraphicsQueue() noexcept;
    static VkQueue GetPresentQueue() noexcept;

    static std::mutex &GetGraphicsMutex() noexcept;
    static std::mutex &GetPresentMutex() noexcept;

    static void LockGraphicsAndPresentQueues() noexcept;
    static void UnlockGraphicsAndPresentQueues() noexcept;

    template <Dimension D> static VkPipelineLayout GetGraphicsPipelineLayout() noexcept;

#ifdef TKIT_ENABLE_VULKAN_PROFILING
    static TKit::VkProfilingContext GetProfilingContext() noexcept;
#endif
};

} // namespace Onyx
