#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/container/static_array.hpp"

#ifndef ONYX_MAX_FRAMES_IN_FLIGHT
#    define ONYX_MAX_FRAMES_IN_FLIGHT 2
#endif

namespace ONYX
{
class ONYX_API SwapChain
{
    KIT_NON_COPYABLE(SwapChain)
  public:
    static constexpr u32 MFIF = ONYX_MAX_FRAMES_IN_FLIGHT;

    SwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, VkPresentModeKHR p_PresentMode,
              const SwapChain *p_OldSwapChain = nullptr) noexcept;
    ~SwapChain() noexcept;

    /**
     * @brief Acquire the next available image from the swap chain.
     *
     * @param p_ImageIndex Pointer to store the index of the acquired image.
     * @return VkResult indicating success or failure.
     */
    VkResult AcquireNextImage(u32 *p_ImageIndex) const noexcept;

    /**
     * @brief Submit a command buffer for execution and signals when rendering is finished.
     *
     * Note: This method could actually submit more than one command buffer at a time, but currently only one is needed.
     *
     * @param p_CommandBuffers The command buffer to submit.
     * @param p_ImageIndex The index of the swap chain image being rendered to.
     * @return VkResult indicating success or failure.
     */
    VkResult SubmitCommandBuffer(const VkCommandBuffer p_CommandBuffers, u32 p_ImageIndex) noexcept;

    /**
     * @brief Presents the rendered image to the screen.
     *
     * @param p_ImageIndex Pointer to the index of the swap chain image to present.
     * @return VkResult indicating success or failure.
     */
    VkResult Present(const u32 *p_ImageIndex) noexcept;

    /**
     * @brief Retrieve the render pass associated with the swap chain.
     *
     * @return The Vulkan render pass.
     */
    VkRenderPass GetRenderPass() const noexcept;

    /**
     * @brief Retrieve the framebuffer at the specified index.
     *
     * @param p_Index The index of the framebuffer to retrieve.
     * @return The Vulkan framebuffer.
     */
    VkFramebuffer GetFrameBuffer(u32 p_Index) const noexcept;

    /**
     * @brief Get the extent (width and height) of the swap chain images.
     *
     * @return The Vulkan extent.
     */
    VkExtent2D GetExtent() const noexcept;

    /**
     * @brief Get the width of the swap chain images.
     *
     * @return The width in pixels.
     */
    u32 GetWidth() const noexcept;

    /**
     * @brief Get the height of the swap chain images.
     *
     * @return The height in pixels.
     */
    u32 GetHeight() const noexcept;

    /**
     * @brief Calculates the aspect ratio of the swap chain images.
     *
     * @return The aspect ratio (width divided by height).
     */
    f32 GetAspectRatio() const noexcept;

    /**
     * @brief Checks if two swap chains are compatible.
     *
     * @param p_SwapChain1 The first swap chain.
     * @param p_SwapChain2 The second swap chain.
     * @return True if compatible, false otherwise.
     */
    static bool AreCompatible(const SwapChain &p_SwapChain1, const SwapChain &p_SwapChain2) noexcept;

  private:
    void createSwapChain(VkExtent2D p_WindowExtent, VkSurfaceKHR p_Surface, VkPresentModeKHR p_PresentMode,
                         const SwapChain *p_OldSwapChain) noexcept;
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