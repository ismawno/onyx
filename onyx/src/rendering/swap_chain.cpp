#include "core/pch.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/core/core.hpp"

#include "onyx/core/glm.hpp"

namespace ONYX
{
static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const DynamicArray<VkSurfaceFormatKHR> &p_Formats) noexcept
{
    for (const VkSurfaceFormatKHR &format : p_Formats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            return format;
    return p_Formats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const DynamicArray<VkPresentModeKHR> &p_Modes) noexcept
{
    for (const VkPresentModeKHR &mode : p_Modes)
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            KIT_LOG_INFO("Present mode: Mailbox");
            return mode;
        }

    for (const VkPresentModeKHR &mode : p_Modes)
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            KIT_LOG_INFO("Present mode: Immediate");
            return mode;
        }
    KIT_LOG_INFO("Present mode: V-Sync");
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &p_Capabilities, VkExtent2D p_WindowExtent) noexcept
{
    if (p_Capabilities.currentExtent.width != UINT32_MAX)
        return p_Capabilities.currentExtent;

    VkExtent2D actualExtent = {p_WindowExtent.width, p_WindowExtent.height};
    actualExtent.width =
        glm::clamp(actualExtent.width, p_Capabilities.minImageExtent.width, p_Capabilities.maxImageExtent.width);

    actualExtent.height =
        glm::clamp(actualExtent.height, p_Capabilities.minImageExtent.height, p_Capabilities.maxImageExtent.height);

    return actualExtent;
}

SwapChain::SwapChain(const VkExtent2D p_WindowExtent, const VkSurfaceKHR p_Surface,
                     const SwapChain *p_OldSwapChain) noexcept
{
    m_Device = Core::GetDevice();
    initialize(p_WindowExtent, p_Surface, p_OldSwapChain);
}

SwapChain::~SwapChain()
{
    for (VkImageView imageView : m_ImageViews)
        vkDestroyImageView(m_Device->VulkanDevice(), imageView, nullptr);
    m_ImageViews.clear();

    if (m_SwapChain)
    {
        vkDestroySwapchainKHR(m_Device->VulkanDevice(), m_SwapChain, nullptr);
        m_SwapChain = nullptr;
    }

    for (usize i = 0; i < m_DepthImages.size(); ++i)
    {
        vkDestroyImageView(m_Device->VulkanDevice(), m_DepthImageViews[i], nullptr);
        vkDestroyImage(m_Device->VulkanDevice(), m_DepthImages[i], nullptr);
        vkFreeMemory(m_Device->VulkanDevice(), m_DepthImageMemories[i], nullptr);
    }

    for (auto frameBuffer : m_Framebuffers)
        vkDestroyFramebuffer(m_Device->VulkanDevice(), frameBuffer, nullptr);

    vkDestroyRenderPass(m_Device->VulkanDevice(), m_RenderPass, nullptr);

    // cleanup synchronization objects
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(m_Device->VulkanDevice(), m_RenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_Device->VulkanDevice(), m_ImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_Device->VulkanDevice(), m_InFlightFences[i], nullptr);
    }
}

void SwapChain::initialize(const VkExtent2D p_WindowExtent, const VkSurfaceKHR p_Surface,
                           const SwapChain *p_OldSwapChain) noexcept
{
    const Device::SwapChainSupportDetails &support = m_Device->SwapChainSupport();

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.Formats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(support.PresentModes);
    const VkExtent2D extent = chooseSwapExtent(support.Capabilities, p_WindowExtent);

    u32 imageCount = support.Capabilities.minImageCount + 1;
    if (support.Capabilities.maxImageCount > 0 && imageCount > support.Capabilities.maxImageCount)
        imageCount = support.Capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = p_Surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const Device::QueueFamilyIndices &indices = m_Device->QueueFamilies();
    std::array<u32, 2> families = {indices.GraphicsFamily, indices.PresentFamily};

    if (indices.GraphicsFamily != indices.PresentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = families.data();
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = support.Capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = p_OldSwapChain ? p_OldSwapChain->m_SwapChain : VK_NULL_HANDLE;

    KIT_ASSERT_RETURNS(vkCreateSwapchainKHR(m_Device->VulkanDevice(), &createInfo, nullptr, &m_SwapChain), VK_SUCCESS,
                       "Failed to create swap chain");

    // We only specified a minimum number of images in the swap chain, so the implementation is
    // allowed to create a swap chain with more. That's why we'll first query the final number of
    // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
    // retrieve the handles.
    vkGetSwapchainImagesKHR(m_Device->VulkanDevice(), m_SwapChain, &imageCount, nullptr);
    m_Images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device->VulkanDevice(), m_SwapChain, &imageCount, m_Images.data());

    m_ImageFormat = surfaceFormat.format;
    m_Extent = extent;
}

void SwapChain::createImageViews() noexcept
{
    m_ImageViews.resize(m_Images.size());

    for (usize i = 0; i < m_Images.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_ImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        KIT_ASSERT_RETURNS(vkCreateImageView(m_Device->VulkanDevice(), &viewInfo, nullptr, &m_ImageViews[i]),
                           VK_SUCCESS, "Failed to create image views");
    }
}

void SwapChain::createRenderPass() noexcept
{
    std::array<VkFormat, 3> formats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    m_DepthFormat =
        m_Device->FindSupportedFormat(formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_DepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_ImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    KIT_ASSERT_RETURNS(vkCreateRenderPass(m_Device->VulkanDevice(), &renderPassInfo, nullptr, &m_RenderPass),
                       VK_SUCCESS, "Failed to create render pass");
}

void SwapChain::createDepthResources() noexcept
{
    m_DepthImages.resize(m_Images.size());
    m_DepthImageViews.resize(m_Images.size());
    m_DepthImageMemories.resize(m_Images.size());

    for (usize i = 0; i < m_DepthImages.size(); ++i)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Extent.width;
        imageInfo.extent.height = m_Extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_DepthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0;

        const auto [image, memory] = m_Device->CreateImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_DepthImages[i] = image;
        m_DepthImageMemories[i] = memory;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_DepthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_DepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        KIT_ASSERT_RETURNS(vkCreateImageView(m_Device->VulkanDevice(), &viewInfo, nullptr, &m_DepthImageViews[i]),
                           VK_SUCCESS, "Failed to create texture image view");
    }
}

void SwapChain::createFramebuffers() noexcept
{
    m_Framebuffers.resize(m_Images.size());
    for (usize i = 0; i < m_Images.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {m_ImageViews[i], m_DepthImageViews[i]};

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass = m_RenderPass;
        frameBufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        frameBufferInfo.pAttachments = attachments.data();
        frameBufferInfo.width = m_Extent.width;
        frameBufferInfo.height = m_Extent.height;
        frameBufferInfo.layers = 1;

        KIT_ASSERT_RETURNS(vkCreateFramebuffer(m_Device->VulkanDevice(), &frameBufferInfo, nullptr, &m_Framebuffers[i]),
                           VK_SUCCESS, "Failed to create frame buffer");
    }
}

void SwapChain::createSyncObjects()
{
    m_InFlightImages.resize(m_Images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        KIT_ASSERT_RETURNS(
            vkCreateSemaphore(m_Device->VulkanDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]),
            VK_SUCCESS, "Failed to create synchronization objects for a frame");
        KIT_ASSERT_RETURNS(
            vkCreateSemaphore(m_Device->VulkanDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]),
            VK_SUCCESS, "Failed to create synchronization objects for a frame");
        KIT_ASSERT_RETURNS(vkCreateFence(m_Device->VulkanDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]),
                           VK_SUCCESS, "Failed to create synchronization objects for a frame");
    }
}

} // namespace ONYX