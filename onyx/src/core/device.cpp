#include "core/pch.hpp"
#include "onyx/core/device.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
#ifdef KIT_OS_APPLE
static const std::array<const char *, 2> s_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                               "VK_KHR_portability_subset"};
#else
static const std::array<const char *, 1> s_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif

// I could use a stack-allocated buffer for the majority of the dynamic allocations there are. But this is a one-time
// initialization setup. It is not worth the effort

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR Capabilities;
    DynamicArray<VkSurfaceFormatKHR> Formats;
    DynamicArray<VkPresentModeKHR> PresentModes;
};

struct QueueFamilyIndices
{
    u32 GraphicsFamily;
    u32 PresentFamily;
    bool GraphicsFamilyHasValue = false;
    bool PresentFamilyHasValue = false;
    bool IsComplete() const noexcept
    {
        return GraphicsFamilyHasValue && PresentFamilyHasValue;
    }
};

static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice p_Device, const VkSurfaceKHR p_Surface)
{
    QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &queueFamilyCount, nullptr);

    DynamicArray<VkQueueFamilyProperties> queue_families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &queueFamilyCount, queue_families.data());

    for (u32 i = 0; i < queue_families.size(); i++)
    {
        if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.GraphicsFamily = i;
            indices.GraphicsFamilyHasValue = true;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(p_Device, i, p_Surface, &presentSupport);
        if (queue_families[i].queueCount > 0 && presentSupport)
        {
            indices.PresentFamily = i;
            indices.PresentFamilyHasValue = true;
        }
        if (indices.IsComplete())
            break;
    }

    return indices;
}

bool checkDeviceExtensionSupport(const VkPhysicalDevice p_Device)
{
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(p_Device, nullptr, &extensionCount, nullptr);

    DynamicArray<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(p_Device, nullptr, &extensionCount, availableExtensions.data());

    HashSet<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

    for (const auto &extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();
}

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice p_Device, const VkSurfaceKHR p_Surface)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_Device, p_Surface, &details.Capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(p_Device, p_Surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.Formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(p_Device, p_Surface, &formatCount, details.Formats.data());
    }

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(p_Device, p_Surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.PresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(p_Device, p_Surface, &presentModeCount, details.PresentModes.data());
    }

    return details;
}

Device::Device(const VkSurfaceKHR p_Surface) noexcept
{
    m_Instance = Core::GetInstance();
    pickPhysicalDevice(p_Surface);
    createLogicalDevice(p_Surface);
    createCommandPool(p_Surface);
}

Device::~Device() noexcept
{
    vkDeviceWaitIdle(m_Device);
    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vkDestroyDevice(m_Device, nullptr);
}

bool Device::IsDeviceSuitable(const VkPhysicalDevice p_Device, const VkSurfaceKHR p_Surface) noexcept
{
    const QueueFamilyIndices indices = findQueueFamilies(p_Device, p_Surface);
    const bool extensionsSupported = checkDeviceExtensionSupport(p_Device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(p_Device, p_Surface);
        swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(p_Device, &supportedFeatures);

    return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool Device::IsSuitable(const VkSurfaceKHR p_Surface) const noexcept
{
    return IsDeviceSuitable(m_PhysicalDevice, p_Surface);
}

void Device::pickPhysicalDevice(const VkSurfaceKHR p_Surface) noexcept
{
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance->VulkanInstance(), &deviceCount, nullptr);
    KIT_ASSERT(deviceCount != 0, "Failed to find GPUs with Vulkan support");

    KIT_LOG_INFO("Device count: {}", deviceCount);
    DynamicArray<VkPhysicalDevice> devices(deviceCount);

    vkEnumeratePhysicalDevices(m_Instance->VulkanInstance(), &deviceCount, devices.data());

    for (const auto &device : devices)
        if (IsDeviceSuitable(device, p_Surface))
        {
            m_PhysicalDevice = device;
            break;
        }

    KIT_ASSERT(m_PhysicalDevice != VK_NULL_HANDLE, "Failed to find a suitable GPU");

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
    KIT_LOG_INFO("Physical device: {}", properties.deviceName);
}

void Device::createLogicalDevice(const VkSurfaceKHR p_Surface) noexcept
{
    const QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice, p_Surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const std::unordered_set<std::uint32_t> uniqueQueueFamily = {indices.GraphicsFamily, indices.PresentFamily};

    const float queuePriority = 1.0f;
    for (std::uint32_t queue_family : uniqueQueueFamily)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queue_family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &device_features;
    createInfo.enabledExtensionCount = static_cast<u32>(s_DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

    // might not really be necessary anymore because device specific validation layers
    // have been deprecated
    const char *vLayer = Instance::ValidationLayer();
#ifdef KIT_ENABLE_ASSERTS
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &vLayer;
#else
    createInfo.enabledLayerCount = 0;
#endif

    KIT_ASSERT_RETURNS(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device), VK_SUCCESS,
                       "Failed to create logical device");

    vkGetDeviceQueue(m_Device, indices.GraphicsFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, indices.PresentFamily, 0, &m_PresentQueue);
}

void Device::createCommandPool(const VkSurfaceKHR p_Surface) noexcept
{
    const QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_PhysicalDevice, p_Surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    KIT_ASSERT_RETURNS(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool), VK_SUCCESS,
                       "Failed to create command pool");
}

} // namespace ONYX