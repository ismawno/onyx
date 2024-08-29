#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"

namespace ONYX
{
class SwapChain
{
    KIT_NON_COPYABLE(SwapChain)
  public:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain = nullptr) noexcept;
    ~SwapChain() noexcept;

  private:
    void initialize(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain) noexcept;
    void createImageViews() noexcept;
    void createRenderPass() noexcept;
    void createDepthResources() noexcept;
    void createFramebuffers() noexcept;
    void createSyncObjects() noexcept;

    KIT::Ref<Device> m_Device;
    VkSwapchainKHR m_SwapChain;
    VkRenderPass m_RenderPass;
    VkExtent2D m_Extent;

    VkFormat m_DepthFormat;
    VkFormat m_ImageFormat;

    DynamicArray<VkImage> m_Images;         // Swap chain images
    DynamicArray<VkImageView> m_ImageViews; // Swap chain image views
    DynamicArray<VkImage> m_DepthImages;
    DynamicArray<VkImageView> m_DepthImageViews;
    DynamicArray<VkDeviceMemory> m_DepthImageMemories;
    DynamicArray<VkFence> m_InFlightImages;

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_RenderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_InFlightFences;

    DynamicArray<VkFramebuffer> m_Framebuffers;
};
} // namespace ONYX