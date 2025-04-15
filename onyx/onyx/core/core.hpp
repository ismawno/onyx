#pragma once

#include "vkit/core/vma.hpp"
#include "vkit/backend/physical_device.hpp"
#include "vkit/descriptors/descriptor_pool.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"
#include "vkit/backend/command_pool.hpp"
#include "tkit/profiling/vulkan.hpp"

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 1024
#endif

#ifndef ONYX_MAX_DESCRIPTORS
#    define ONYX_MAX_DESCRIPTORS 1024
#endif

#ifndef ONYX_MAX_FRAMES_IN_FLIGHT
#    define ONYX_MAX_FRAMES_IN_FLIGHT 2
#endif

#ifndef ONYX_MAX_WORKER_THREADS
#    define ONYX_MAX_WORKER_THREADS 15
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

    static VkQueue GetGraphicsQueue() noexcept;
    static VkQueue GetPresentQueue() noexcept;

    static VkPipelineLayout GetGraphicsPipelineLayoutSimple() noexcept;
    static VkPipelineLayout GetGraphicsPipelineLayoutComplex() noexcept;

#ifdef TKIT_ENABLE_VULKAN_PROFILING
    static TKit::VkProfilingContext GetProfilingContext() noexcept;
#endif
};

} // namespace Onyx
