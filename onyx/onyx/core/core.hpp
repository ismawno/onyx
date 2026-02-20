#pragma once

#include "onyx/core/alias.hpp"
#include "vkit/memory/allocator.hpp"
#include "vkit/vulkan/instance.hpp"
#include "vkit/vulkan/loader.hpp"
#include "vkit/device/logical_device.hpp"
#include "vkit/device/physical_device.hpp"
#include "vkit/core/core.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
#    include "tkit/profiling/vulkan.hpp"
#endif

#define ONYX_NO_DISCARD VKIT_NO_DISCARD

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

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
using Task = TKit::Task<void>;
using enum VKit::ErrorCode;
template <typename T = void> using Result = VKit::Result<T>;

struct InitCallbacks
{
    std::function<void(VKit::Instance::Builder &)> OnInstanceCreation = nullptr;
    std::function<void(VKit::PhysicalDevice::Selector &)> OnPhysicalDeviceCreation = nullptr;
    std::function<void(VKit::LogicalDevice::Builder &)> OnLogicalDeviceCreation = nullptr;
    std::function<void(VKit::AllocatorSpecs &)> OnAllocatorCreation = nullptr;
};

namespace Platform
{
struct Specs;
}
namespace Execution
{
struct Specs;
}
namespace Assets
{
struct Specs;
}
namespace Descriptors
{
struct Specs;
}
namespace Shaders
{
struct Specs;
}

struct Specs
{
    const char *VulkanLoaderPath = nullptr;
    TKit::ITaskManager *TaskManager = nullptr;
    TKit::FixedArray<u32, VKit::Queue_Count> QueueRequests{1, 0, 1, 1};
    TKit::FixedArray<VKit::Allocation, TKit::MaxThreads> Allocators{};
    InitCallbacks Callbacks{};
    const char *Locale = "en_US.UTF-8";
    Execution::Specs *ExecutionSpecs = nullptr;
    Assets::Specs *AssetSpecs = nullptr;
    Descriptors::Specs *DescriptorSpecs = nullptr;
    Shaders::Specs *ShadersSpecs = nullptr;
    Platform::Specs *PlatformSpecs = nullptr;
#ifdef TKIT_ENABLE_ASSERTS
    bool EnableValidationLayers = true;
#else
    bool EnableValidationLayers = false;
#endif
};

} // namespace Onyx

namespace Onyx::Core
{
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs = {});
void Terminate();

TKit::ITaskManager *GetTaskManager();
void SetTaskManager(TKit::ITaskManager *taskManager);

const VKit::Instance &GetInstance();
const VKit::Vulkan::InstanceTable *GetInstanceTable();

const VKit::LogicalDevice &GetDevice();
const VKit::Vulkan::DeviceTable *GetDeviceTable();

ONYX_NO_DISCARD Result<> DeviceWaitIdle();

VmaAllocator GetVulkanAllocator();
VKit::DeletionQueue &GetDeletionQueue();

}; // namespace Onyx::Core
