#pragma once

#include "onyx/core/limits.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/asset/assets.hpp"
#include "vkit/memory/allocator.hpp"
#include "vkit/vulkan/instance.hpp"
#include "vkit/vulkan/loader.hpp"
#include "vkit/device/logical_device.hpp"
#include "vkit/device/physical_device.hpp"
#include "vkit/core/core.hpp"
#include "tkit/multiprocessing/task.hpp"
#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
#    include "tkit/profiling/vulkan.hpp"
#endif

#define ONYX_PLATFORM_ANY 0x00060000
#define ONYX_PLATFORM_WIN32 0x00060001
#define ONYX_PLATFORM_COCOA 0x00060002
#define ONYX_PLATFORM_WAYLAND 0x00060003
#define ONYX_PLATFORM_X11 0x00060004
#define ONYX_PLATFORM_NULL 0x00060005

#ifdef TKIT_OS_LINUX
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_X11
#elif defined(TKIT_OS_APPLE)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_COCOA
#elif defined(TKIT_OS_WINDOWS)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_WIN32
#endif

#ifndef ONYX_PLATFORM_AUTO
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_ANY
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

struct InitCallbacks
{
    std::function<void(VKit::Instance::Builder &)> OnInstanceCreation = nullptr;
    std::function<void(VKit::PhysicalDevice::Selector &)> OnPhysicalDeviceCreation = nullptr;
    std::function<void(VKit::LogicalDevice::Builder &)> OnLogicalDeviceCreation = nullptr;
    std::function<void(VKit::AllocatorSpecs &)> OnAllocatorCreation = nullptr;
};

struct Specs
{
    const char *VulkanLoaderPath = nullptr;
    TKit::ITaskManager *TaskManager = nullptr;
    TKit::FixedArray<u32, VKit::Queue_Count> QueueRequests{4, 0, 4, 1};
    TKit::FixedArray<VKit::Allocation, TKit::MaxThreads> Allocators{};
    InitCallbacks Callbacks{};
    Execution::Specs ExecutionSpecs{};
    Assets::Specs AssetSpecs{};
    Descriptors::Specs DescriptorSpecs{};
    Shaders::Specs ShadersSpecs{};
    u32 Platform = ONYX_PLATFORM_AUTO;
};

} // namespace Onyx

namespace Onyx::Core
{
void Initialize(const Specs &p_Specs = {});
void Terminate();

TKit::ITaskManager *GetTaskManager();
void SetTaskManager(TKit::ITaskManager *p_TaskManager);

const VKit::Instance &GetInstance();
const VKit::Vulkan::InstanceTable *GetInstanceTable();

const VKit::LogicalDevice &GetDevice();
const VKit::Vulkan::DeviceTable *GetDeviceTable();

void CreateDevice(VkSurfaceKHR p_Surface);

bool IsDeviceCreated();
void DeviceWaitIdle();

VmaAllocator GetVulkanAllocator();
VKit::DeletionQueue &GetDeletionQueue();

}; // namespace Onyx::Core
