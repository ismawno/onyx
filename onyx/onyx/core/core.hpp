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
            const auto r = HandleVulkanResult(error.GetVulkanResult());
            VKIT_LOG_RESULT_ERROR(r);
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
#ifdef ONYX_ENABLE_SHADER_API
namespace Shaders
{
struct Specs;
}
#endif
namespace Renderer
{
struct Specs;
}
namespace Platform
{
struct Specs;
}

using InitializationFlags = u16;
enum InitializationFlagBit : InitializationFlags
{
    InitializationFlag_DefaultTaskManagerSingleThread = 1 << 0,
    InitializationFlag_EnableValidationLayers = 1 << 1,
    InitializationFlag_EnableDebugUtilsExtension = 1 << 2,
    InitializationFlag_EnableDeviceAssistedDebugFeature = 1 << 3,
    InitializationFlag_EnableBestPracticesDebugFeature = 1 << 4,
    InitializationFlag_EnablePrintfDebugFeature = 1 << 5,
    InitializationFlag_EnableSyncValidationDebugFeature = 1 << 6,
    InitializationFlag_EnableDeviceFaultExtension = 1 << 7,
    InitializationFlag_ResetArenasOnTermination = 1 << 8
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
#ifdef ONYX_ENABLE_SHADER_API
    Shaders::Specs *ShadersSpecs = nullptr;
#endif
    Renderer::Specs *RendererSpecs = nullptr;
    Platform::Specs *PlatformSpecs = nullptr;
#ifdef TKIT_ENABLE_ASSERTS
#    ifdef TKIT_OS_APPLE
    InitializationFlags Flags =
        InitializationFlag_EnableValidationLayers | InitializationFlag_EnableDebugUtilsExtension |
        InitializationFlag_EnableBestPracticesDebugFeature | InitializationFlag_EnableSyncValidationDebugFeature |
        InitializationFlag_EnableDeviceFaultExtension;
#    else
    // TODO(Isma): Revert this
    //  InitializationFlags Flags =
    //      InitializationFlag_EnableValidationLayers | InitializationFlag_EnableDebugUtilsExtension |
    //      InitializationFlag_EnableBestPracticesDebugFeature | InitializationFlag_EnableSyncValidationDebugFeature |
    //      InitializationFlag_EnableDeviceAssistedDebugFeature | InitializationFlag_EnableDeviceFaultExtension;
    InitializationFlags Flags =
        InitializationFlag_EnableValidationLayers | InitializationFlag_EnableDebugUtilsExtension |
        InitializationFlag_EnableBestPracticesDebugFeature | InitializationFlag_EnableSyncValidationDebugFeature |
        InitializationFlag_EnablePrintfDebugFeature | InitializationFlag_EnableDeviceFaultExtension;
#    endif
#else
    Flags Flags = 0;
#endif
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs = {});
void Terminate();

TKit::ArenaAllocator *GetArena(u32 threadIndex = 0);
TKit::StackAllocator *GetStack(u32 threadIndex = 0);
TKit::TierAllocator *GetTier(u32 threadIndex = 0);

void PushArena(u32 threadIndex = 0);
void PushStack(u32 threadIndex = 0);
void PushTier(u32 threadIndex = 0);

TKit::ITaskManager *GetTaskManager();

const VKit::Instance &GetInstance();
const VKit::Vulkan::InstanceTable *GetInstanceTable();

const VKit::LogicalDevice &GetDevice();
const VKit::Vulkan::DeviceTable *GetDeviceTable();

bool IsDebugUtilsEnabled();

ONYX_NO_DISCARD Result<> DeviceWaitIdle();

VmaAllocator GetVulkanAllocator();
VKit::DeletionQueue &GetDeletionQueue();

}; // namespace Onyx
