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

#ifndef ONYX_MAX_TASKS
#    define ONYX_MAX_TASKS 256
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
using Task = TKit::Task<void>;
using TaskArray = TKit::StaticArray<Task, ONYX_MAX_TASKS>;

class Initializer
{
  public:
    virtual ~Initializer() = default;

    virtual void OnInstanceCreation(VKit::Instance::Builder &)
    {
    }
    virtual void OnPhysicalDeviceCreation(VKit::PhysicalDevice::Selector &)
    {
    }
    virtual void OnAllocatorCreation(VKit::AllocatorSpecs &)
    {
    }
};

enum class TransferMode : u8
{
    Separate,
    SameIndex,
    SameQueue
};

struct Specs
{
    TKit::ITaskManager *TaskManager = nullptr;
    Initializer *Initializer = nullptr;
};

template <typename T> using PerFrameData = TKit::Array<T, ONYX_MAX_FRAMES_IN_FLIGHT>;
template <typename T> using PerImageData = TKit::StaticArray8<T>;
struct ONYX_API Core
{
    static void Initialize(const Specs &p_Specs = {});
    static void Terminate();

    static TKit::ITaskManager *GetTaskManager();
    static void SetTaskManager(TKit::ITaskManager *p_TaskManager);

    static const VKit::Instance &GetInstance();
    static const VKit::Vulkan::InstanceTable &GetInstanceTable();

    static const VKit::LogicalDevice &GetDevice();
    static const VKit::Vulkan::DeviceTable &GetDeviceTable();

    static void CreateDevice(VkSurfaceKHR p_Surface);

    static bool IsDeviceCreated();
    static void DeviceWaitIdle();

    static VKit::CommandPool &GetGraphicsPool();
    static VKit::CommandPool &GetTransferPool();
    static VmaAllocator GetVulkanAllocator();

    static VKit::DeletionQueue &GetDeletionQueue();

    static const VKit::DescriptorPool &GetDescriptorPool();
    static const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout();
    static const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout();

    static VkQueue GetGraphicsQueue();
    static VkQueue GetPresentQueue();
    static VkQueue GetTransferQueue();

    static u32 GetGraphicsIndex();
    static u32 GetPresentIndex();
    static u32 GetTransferIndex();

    static TransferMode GetTransferMode();

    static VkPipelineLayout GetGraphicsPipelineLayoutSimple();
    static VkPipelineLayout GetGraphicsPipelineLayoutComplex();

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
    static TKit::VkProfilingContext GetGraphicsContext();
    // static TKit::VkProfilingContext GetTransferContext();
#endif
};

} // namespace Onyx
