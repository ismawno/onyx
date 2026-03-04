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
#define ONYX_CHECK_EXPRESSION(expression) Onyx::CheckExpression(expression)

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

ONYX_NO_DISCARD Result<> HandleVulkanResult(const VkResult result);

template <typename T, typename E> T CheckExpression(TKit::Result<T, E> &&result)
{
#ifdef TKIT_ENABLE_ASSERTS
    if (!result)
    {
        const auto &error = result.GetError();
        if (error.GetCode() == Error_VulkanError)
        {
            const auto result = HandleVulkanResult(error.GetVulkanResult());
            VKIT_LOG_RESULT_ERROR(result);
        }

        TKIT_FATAL("{}", error.ToString());
    }
#endif
    if constexpr (!std::same_as<T, void>)
        return result.GetValue();
}

#ifdef TKIT_ENABLE_ASSERTS
inline void CheckExpression(const VkResult result)
{
    const auto res = HandleVulkanResult(result);
    VKIT_LOG_RESULT_ERROR(res);
    VKIT_CHECK_RESULT(result);
}
#else
inline void CheckExpression(const VkResult)
{
}
#endif

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

using Flags = u8;
enum FlagBit : Flags
{
    Flag_DefaultTaskManagerSingleThread = 1 << 0,
    Flag_EnableValidationLayers = 1 << 1,
    Flag_EnableDebugUtilsExtension = 1 << 2,
    Flag_EnableDeviceAssistedDebugFeature = 1 << 3,
    Flag_EnableBestPracticesDebugFeature = 1 << 4,
    Flag_EnablePrintfDebugFeature = 1 << 5,
    Flag_EnableSyncValidationDebugFeature = 1 << 6,
    Flag_EnableDeviceFaultExtension = 1 << 7
};

struct Specs
{
    const char *VulkanLoaderPath = nullptr;
    const char *Locale = "en_US.UTF-8";
    // null uses tmp dir. only relevant when device fault extension is enabled
    const char *DeviceFaultCrashDump = nullptr;
    TKit::ITaskManager *TaskManager = nullptr;
    TKit::FixedArray<u32, VKit::Queue_Count> QueueRequests{1, 0, 1, 1};
    TKit::FixedArray<VKit::Allocation, TKit::MaxThreads> Allocators{};
    InitCallbacks Callbacks{};
    Execution::Specs *ExecutionSpecs = nullptr;
    Assets::Specs *AssetSpecs = nullptr;
    Descriptors::Specs *DescriptorSpecs = nullptr;
    Shaders::Specs *ShadersSpecs = nullptr;
    Platform::Specs *PlatformSpecs = nullptr;
#ifdef TKIT_ENABLE_ASSERTS
    Flags Flags = Flag_EnableValidationLayers | Flag_EnableDebugUtilsExtension | Flag_EnableBestPracticesDebugFeature |
                  Flag_EnableSyncValidationDebugFeature | Flag_EnableDeviceAssistedDebugFeature |
                  Flag_EnableDeviceFaultExtension;
#else
    Flags Flags = 0;
#endif
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs = {});
void Terminate();

} // namespace Onyx

namespace Onyx::Core
{
TKit::ITaskManager *GetTaskManager();

const VKit::Instance &GetInstance();
const VKit::Vulkan::InstanceTable *GetInstanceTable();

const VKit::LogicalDevice &GetDevice();
const VKit::Vulkan::DeviceTable *GetDeviceTable();

bool CanNameObjects();

ONYX_NO_DISCARD Result<> DeviceWaitIdle();

VmaAllocator GetVulkanAllocator();
VKit::DeletionQueue &GetDeletionQueue();

}; // namespace Onyx::Core
