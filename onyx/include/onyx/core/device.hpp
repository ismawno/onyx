#pragma once

#include "onyx/core/instance.hpp"
#include "onyx/core/alias.hpp"
#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API Device : public KIT::RefCounted<Device>
{
    KIT_NON_COPYABLE(Device)
  public:
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
    };

    explicit Device(VkSurfaceKHR p_Surface) noexcept;
    ~Device() noexcept;

    VkFormat FindSupportedFormat(std::span<const VkFormat> p_Candidates, VkImageTiling p_Tiling,
                                 VkFormatFeatureFlags p_Features) const noexcept;

    std::pair<VkImage, VkDeviceMemory> CreateImage(const VkImageCreateInfo &p_Info, VkMemoryPropertyFlags p_Properties);

    bool IsSuitable(VkSurfaceKHR p_Surface) const noexcept;
    VkDevice VulkanDevice() const noexcept;

    const SwapChainSupportDetails &SwapChainSupport() const noexcept;
    const QueueFamilyIndices &QueueFamilies() const noexcept;

    VkQueue GraphicsQueue() const noexcept;
    VkQueue PresentQueue() const noexcept;

  private:
    void pickPhysicalDevice(VkSurfaceKHR p_Surface) noexcept;
    void createLogicalDevice() noexcept;
    void createCommandPool() noexcept;

    static bool isDeviceSuitable(VkPhysicalDevice p_Device, VkSurfaceKHR p_Surface,
                                 SwapChainSupportDetails *p_SwapChainSupport = nullptr,
                                 QueueFamilyIndices *p_QueueFamilies = nullptr) noexcept;

    KIT::Ref<Instance> m_Instance;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device;
    VkCommandPool m_CommandPool;

    SwapChainSupportDetails m_SwapChainSupport;
    QueueFamilyIndices m_QueueFamilies;

    VkQueue m_GraphicsQueue;
    VkQueue m_PresentQueue;
};
} // namespace ONYX