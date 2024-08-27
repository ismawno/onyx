#pragma once

#include "onyx/core/instance.hpp"
#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API Device : public KIT::RefCounted<Device>
{
    KIT_NON_COPYABLE(Device)
  public:
    explicit Device(VkSurfaceKHR p_Surface) noexcept;
    ~Device() noexcept;

    static bool IsDeviceSuitable(VkPhysicalDevice p_Device, VkSurfaceKHR p_Surface) noexcept;
    bool IsSuitable(VkSurfaceKHR p_Surface) const noexcept;

  private:
    void pickPhysicalDevice(VkSurfaceKHR p_Surface) noexcept;
    void createLogicalDevice(VkSurfaceKHR p_Surface) noexcept;
    void createCommandPool(VkSurfaceKHR p_Surface) noexcept;

    KIT::Ref<Instance> m_Instance;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device;
    VkCommandPool m_CommandPool;

    // TODO: Evaluate if these are necessary to stay
    VkQueue m_GraphicsQueue;
    VkQueue m_PresentQueue;
};
} // namespace ONYX