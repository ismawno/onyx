#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/container/static_array.hpp"

namespace ONYX
{
class ONYX_API SwapChain
{
    KIT_NON_COPYABLE(SwapChain)
  public:
    // Maximum frames in flight
    static constexpr u32 MFIF = 2;

    SwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain = nullptr) noexcept;
    ~SwapChain() noexcept;

    VkResult AcquireNextImage(u32 *p_ImageIndex) const noexcept;

    // This method could actually submit more than one command buffer at a time, but for now only one is needed
    VkResult SubmitCommandBuffer(const VkCommandBuffer p_CommandBuffers, u32 p_ImageIndex) noexcept;
    VkResult Present(const u32 *p_ImageIndex) noexcept;

    VkRenderPass GetRenderPass() const noexcept;
    VkFramebuffer GetFrameBuffer(u32 p_Index) const noexcept;
    VkExtent2D GetExtent() const noexcept;

    u32 GetWidth() const noexcept;
    u32 GetHeight() const noexcept;

    f32 GetAspectRatio() const noexcept;

    static bool AreCompatible(const SwapChain &p_SwapChain1, const SwapChain &p_SwapChain2) noexcept;

  private:
    void createSwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, const SwapChain *p_OldSwapChain) noexcept;
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

    std::array<VkSemaphore, MFIF> m_ImageAvailableSemaphores;
    std::array<VkSemaphore, MFIF> m_RenderFinishedSemaphores;
    std::array<VkFence, MFIF> m_InFlightFences;

    KIT::StaticArray<VkFramebuffer, 3> m_Framebuffers;
    usize m_CurrentFrame = 0;
};
} // namespace ONYX