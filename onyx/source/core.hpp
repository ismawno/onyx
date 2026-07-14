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
#ifdef TKIT_ENABLE_ENSURE
    if (!result)
    {
        const auto &error = result.GetError();
        if (error.GetCode() == VKit::Error_VulkanError)
            HandleVulkanResult(error.GetVulkanResult());

        TKIT_PANIC("{}", error.ToString());
    }
#endif

    TKIT_ASSERT(result, "{}", result.GetError().ToString());
    if constexpr (!std::same_as<T, void>)
        return *result;
}

inline void CheckVKitError(const VkResult result)
{
#ifdef TKIT_ENABLE_ENSURE
    HandleVulkanResult(result);
#endif
    VKIT_CHECK_RESULT(result);
}

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
