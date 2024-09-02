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

static bool checkDeviceExtensionSupport(const VkPhysicalDevice p_Device) noexcept
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

static Device::QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice p_Device,
                                                    const VkSurfaceKHR p_Surface) noexcept
{
    Device::QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &queueFamilyCount, nullptr);

    DynamicArray<VkQueueFamilyProperties> queue_families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(p_Device, &queueFamilyCount, queue_families.data());

    for (u32 i = 0; i < queue_families.size(); ++i)
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
        if (indices.PresentFamilyHasValue && indices.GraphicsFamilyHasValue)
            break;
    }

    return indices;
}

static Device::SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice p_Device,
                                                             const VkSurfaceKHR p_Surface) noexcept
{
    Device::SwapChainSupportDetails details;
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

static bool isDeviceSuitable(const VkPhysicalDevice p_Device, const VkSurfaceKHR p_Surface) noexcept
{
    const Device::QueueFamilyIndices indices = findQueueFamilies(p_Device, p_Surface);
    const bool extensionsSupported = checkDeviceExtensionSupport(p_Device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        const Device::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(p_Device, p_Surface);
        swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(p_Device, &supportedFeatures);

    return indices.PresentFamilyHasValue && indices.GraphicsFamilyHasValue && extensionsSupported &&
           swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

Device::Device(const VkSurfaceKHR p_Surface) noexcept
{
    KIT_LOG_INFO("Attempting to create a new device...");
    m_Instance = Core::GetInstance();
    pickPhysicalDevice(p_Surface);
    createLogicalDevice(p_Surface);
}

Device::~Device() noexcept
{
    vkDestroyDevice(m_Device, nullptr);
}

void Device::WaitIdle() const noexcept
{
    vkDeviceWaitIdle(m_Device);
}

bool Device::IsSuitable(const VkSurfaceKHR p_Surface) const noexcept
{
    return isDeviceSuitable(m_PhysicalDevice, p_Surface);
}

VkDevice Device::VulkanDevice() const noexcept
{
    return m_Device;
}

Device::QueueFamilyIndices Device::FindQueueFamilies(const VkSurfaceKHR p_Surface) const noexcept
{
    return findQueueFamilies(m_PhysicalDevice, p_Surface);
}
Device::SwapChainSupportDetails Device::QuerySwapChainSupport(const VkSurfaceKHR p_Surface) const noexcept
{
    return querySwapChainSupport(m_PhysicalDevice, p_Surface);
}

u32 Device::FindMemoryType(const u32 p_TypeFilter, const VkMemoryPropertyFlags p_Properties) const noexcept
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++)
        if ((p_TypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & p_Properties) == p_Properties)
            return i;

    KIT_ERROR("Failed to find suitable memory type");
    return UINT32_MAX;
}

VkQueue Device::GraphicsQueue() const noexcept
{
    return m_GraphicsQueue;
}
VkQueue Device::PresentQueue() const noexcept
{
    return m_PresentQueue;
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
        if (isDeviceSuitable(device, p_Surface))
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
    const QueueFamilyIndices indices = FindQueueFamilies(p_Surface);
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

VkFormat Device::FindSupportedFormat(const std::span<const VkFormat> p_Candidates, const VkImageTiling p_Tiling,
                                     const VkFormatFeatureFlags p_Features) const noexcept
{
    for (const VkFormat candidate : p_Candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, candidate, &props);

        if (p_Tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & p_Features) == p_Features)
            return candidate;
        if (p_Tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & p_Features) == p_Features)
            return candidate;
    }

    KIT_ASSERT(false, "Failed to find a supported format");
    return VK_FORMAT_UNDEFINED;
}

} // namespace ONYX