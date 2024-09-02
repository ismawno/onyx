#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/container/static_array.hpp"

namespace ONYX
{
class SwapChain
{
    KIT_NON_COPYABLE(SwapChain)
  public:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain = nullptr) noexcept;
    ~SwapChain() noexcept;

    VkResult AcquireNextImage(u32 *p_ImageIndex) const noexcept;
    VkResult SubmitCommandBuffers(std::span<const VkCommandBuffer> p_CommandBuffers, u32 p_ImageIndex) noexcept;
    VkResult Present(const u32 *p_ImageIndex) noexcept;

    VkRenderPass RenderPass() const noexcept;
    VkFramebuffer FrameBuffer(u32 p_Index) const noexcept;
    VkExtent2D Extent() const noexcept;

    u32 Width() const noexcept;
    u32 Height() const noexcept;

    f32 AspectRatio() const noexcept;

    static bool AreCompatible(const SwapChain &p_SwapChain1, const SwapChain &p_SwapChain2) noexcept;

  private:
    void initialize(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain) noexcept;
    void createImageViews() noexcept;
    void createRenderPass() noexcept;
    void createDepthResources() noexcept;
    void createFrameBuffers() noexcept;
    void createSyncObjects() noexcept;

    std::pair<VkImage, VkDeviceMemory> createImage(const VkImageCreateInfo &p_Info,
                                                   const VkMemoryPropertyFlags p_Properties);

    KIT::Ref<Device> m_Device;
    VkSwapchainKHR m_SwapChain;
    VkRenderPass m_RenderPass;
    VkExtent2D m_Extent;

    VkFormat m_DepthFormat;
    VkFormat m_ImageFormat;

    KIT::StaticArray<VkImage, 3> m_Images;         // Swap chain images
    KIT::StaticArray<VkImageView, 3> m_ImageViews; // Swap chain image views
    KIT::StaticArray<VkImage, 3> m_DepthImages;
    KIT::StaticArray<VkImageView, 3> m_DepthImageViews;
    KIT::StaticArray<VkDeviceMemory, 3> m_DepthImageMemories;
    KIT::StaticArray<VkFence, 3> m_InFlightImages;

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_RenderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_InFlightFences;

    KIT::StaticArray<VkFramebuffer, 3> m_Framebuffers;
    usize m_CurrentFrame = 0;
};
} // namespace ONYX