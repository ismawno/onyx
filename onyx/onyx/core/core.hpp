#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"
#include "vkit/vulkan/allocator.hpp"
#include "vkit/descriptors/descriptor_pool.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"
#include "vkit/rendering/command_pool.hpp"
#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
#    include "tkit/profiling/vulkan.hpp"
#endif
#include "vkit/vulkan/instance.hpp"
#include "vkit/vulkan/loader.hpp"
#include "vkit/vulkan/logical_device.hpp"
#include "vkit/vulkan/physical_device.hpp"

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 1024
#endif

#ifndef ONYX_MAX_DESCRIPTORS
#    define ONYX_MAX_DESCRIPTORS 1024
#endif

#ifndef ONYX_MAX_FRAMES_IN_FLIGHT
#    define ONYX_MAX_FRAMES_IN_FLIGHT 2
#endif

#if ONYX_MAX_FRAMES_IN_FLIGHT < 2
#    error "[ONYX] Maximum frames in flight must be greater than 2"
#endif

// The amount of active threads (accounting for the main thread as well) should not surpass this number. This means
// thread pools should be created with LESS threads than this limit.
#ifndef ONYX_MAX_THREADS
#    define ONYX_MAX_THREADS 16
#endif

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

#ifndef ONYX_MAX_TASKS
#    define ONYX_MAX_TASKS 512
#endif

namespace TKit
{
class StackAllocator;
class ITaskManager;
template <typename T> class Task;
} // namespace TKit

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
using Task = TKit::Task<void> *;
using TaskArray = TKit::StaticArray<Task, ONYX_MAX_TASKS>;

class Initializer
{
  public:
    virtual ~Initializer() = default;

    virtual void OnInstanceCreation(VKit::Instance::Builder &) noexcept
    {
    }
    virtual void OnPhysicalDeviceCreation(VKit::PhysicalDevice::Selector &) noexcept
    {
    }
    virtual void OnAllocatorCreation(VKit::AllocatorSpecs &) noexcept
    {
    }
};

enum class TransferMode : u8
{
    Separate,
    SameIndex,
    SameQueue
};

template <typename T> using PerFrameData = TKit::Array<T, ONYX_MAX_FRAMES_IN_FLIGHT>;
struct ONYX_API Core
{
    static void Initialize(TKit::ITaskManager *p_TaskManager, Initializer *p_Initializer = nullptr) noexcept;
    static void Terminate() noexcept;

    static TKit::ITaskManager *GetTaskManager() noexcept;
    static void SetTaskManager(TKit::ITaskManager *p_TaskManager) noexcept;

    static const VKit::Instance &GetInstance() noexcept;
    static const VKit::Vulkan::InstanceTable &GetInstanceTable() noexcept;

    static const VKit::LogicalDevice &GetDevice() noexcept;
    static const VKit::Vulkan::DeviceTable &GetDeviceTable() noexcept;

    static void CreateDevice(VkSurfaceKHR p_Surface) noexcept;

    static bool IsDeviceCreated() noexcept;
    static void DeviceWaitIdle() noexcept;

    static VKit::CommandPool &GetGraphicsPool() noexcept;
    static VKit::CommandPool &GetTransferPool() noexcept;
    static VmaAllocator GetVulkanAllocator() noexcept;

    static VKit::DeletionQueue &GetDeletionQueue() noexcept;

    static const VKit::DescriptorPool &GetDescriptorPool() noexcept;
    static const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout() noexcept;
    static const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout() noexcept;

    static VkQueue GetGraphicsQueue() noexcept;
    static VkQueue GetPresentQueue() noexcept;
    static VkQueue GetTransferQueue() noexcept;

    static u32 GetGraphicsIndex() noexcept;
    static u32 GetPresentIndex() noexcept;
    static u32 GetTransferIndex() noexcept;

    static TransferMode GetTransferMode() noexcept;

    static VkPipelineLayout GetGraphicsPipelineLayoutSimple() noexcept;
    static VkPipelineLayout GetGraphicsPipelineLayoutComplex() noexcept;

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
    static TKit::VkProfilingContext GetGraphicsContext() noexcept;
    // static TKit::VkProfilingContext GetTransferContext() noexcept;
#endif
};

} // namespace Onyx
