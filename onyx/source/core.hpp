#pragma once

#include "onyx/core.hpp"
#include "vkit/memory/allocator.hpp"
#include "vkit/vulkan/instance.hpp"
#include "vkit/vulkan/loader.hpp"
#include "vkit/device/logical_device.hpp"

#define ONYX_CHECK_VKIT_RESULT(expression) Onyx::CheckVKitError(expression)

// This file handles the lifetime of global data the Onyx library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure the Terminate function is called at the end of your program, and that no ONYX
// objects are alive at that point.

namespace Onyx
{
void HandleVulkanResult(const VkResult result);

template <typename T> auto CheckVKitError(TKit::Result<T, VKit::Error> &&result)
{
#ifdef TKIT_ENABLE_ASSERTS
    if (!result)
    {
        const auto &error = result.GetError();
        if (error.GetCode() == VKit::Error_VulkanError)
            HandleVulkanResult(error.GetVulkanResult());

        TKIT_FATAL("{}", error.ToString());
    }
#endif
    if constexpr (!std::same_as<T, void>)
        return *result;
}

#ifdef TKIT_ENABLE_ASSERTS
inline void CheckVKitError(const VkResult result)
{
    HandleVulkanResult(result);
    VKIT_CHECK_RESULT(result);
}
#else
inline void CheckVKitError(const VkResult)
{
}
#endif

const VKit::Instance &GetInstance();
const VKit::Vulkan::InstanceTable *GetInstanceTable();

const VKit::LogicalDevice &GetDevice();
const VKit::Vulkan::DeviceTable *GetDeviceTable();

const VKit::PhysicalDevice &GetPhysicalDevice();

bool IsDebugUtilsEnabled();

void DeviceWaitIdle();

VmaAllocator GetVulkanAllocator();
VKit::DeletionQueue &GetDeletionQueue();

}; // namespace Onyx
