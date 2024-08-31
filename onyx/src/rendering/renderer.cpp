#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/core.hpp"
#include "onyx/drawing/color.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Renderer::Renderer(Window<N> &p_Window) noexcept
{
    m_Device = Core::GetDevice();
    createSwapChain(p_Window);
    createCommandBuffers();
}

Renderer::~Renderer() noexcept
{
    vkDeviceWaitIdle(m_Device->VulkanDevice());
    vkFreeCommandBuffers(m_Device->VulkanDevice(), m_Device->CommandPool(), SwapChain::MAX_FRAMES_IN_FLIGHT,
                         m_CommandBuffers.data());
}

ONYX_DIMENSION_TEMPLATE VkCommandBuffer Renderer::BeginFrame(Window<N> &p_Window) noexcept
{
    KIT_ASSERT(!m_FrameStarted, "Cannot begin a new frame when there is already one in progress");

    const VkResult result = m_SwapChain->AcquireNextImage(&m_ImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        createSwapChain(p_Window);
        return nullptr;
    }

    KIT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image");
    m_FrameStarted = true;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    KIT_ASSERT_RETURNS(vkResetCommandBuffer(m_CommandBuffers[m_FrameIndex], 0), VK_SUCCESS,
                       "Failed to reset command buffer");
    KIT_ASSERT_RETURNS(vkBeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &beginInfo), VK_SUCCESS,
                       "Failed to begin command buffer");

    return m_CommandBuffers[m_FrameIndex];
}

ONYX_DIMENSION_TEMPLATE void Renderer::EndFrame(Window<N> &p_Window) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end a frame when there is no frame in progress");
    KIT_ASSERT_RETURNS(vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]), VK_SUCCESS, "Failed to end command buffer");

    const std::span<const VkCommandBuffer> buffers = {&m_CommandBuffers[m_FrameIndex], 1};
    KIT_ASSERT_RETURNS(m_SwapChain->SubmitCommandBuffers(buffers, m_ImageIndex), VK_SUCCESS,
                       "Failed to submit command buffers");

    const VkResult result = m_SwapChain->Present(&m_ImageIndex);
    const bool resizeFixes = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || p_Window.WasResized();
    if (resizeFixes)
    {
        createSwapChain(p_Window);
        p_Window.FlagResizeDone();
    }

    KIT_ASSERT(resizeFixes || result == VK_SUCCESS, "Failed to submit command buffers");
    m_FrameStarted = false;
    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::BeginRenderPass(const Color &p_ClearColor) noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot begin render pass if a frame is not in progress");
    const VkExtent2D extent = m_SwapChain->Extent();

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_SwapChain->RenderPass();
    passInfo.framebuffer = m_SwapChain->FrameBuffer(m_ImageIndex);
    passInfo.renderArea.offset = {0, 0};
    passInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {{p_ClearColor.RGBA.r, p_ClearColor.RGBA.g, p_ClearColor.RGBA.b, p_ClearColor.RGBA.a}};
    clear_values[1].depthStencil = {1, 0};

    passInfo.clearValueCount = 2;
    passInfo.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(m_CommandBuffers[m_FrameIndex], &passInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {extent.width, extent.height};

    vkCmdSetViewport(m_CommandBuffers[m_FrameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_CommandBuffers[m_FrameIndex], 0, 1, &scissor);
}

void Renderer::EndRenderPass() noexcept
{
    KIT_ASSERT(m_FrameStarted, "Cannot end render pass if a frame is not in progress");
    vkCmdEndRenderPass(m_CommandBuffers[m_FrameIndex]);
}

VkCommandBuffer Renderer::CurrentCommandBuffer() const noexcept
{
    return m_CommandBuffers[m_FrameIndex];
}

ONYX_DIMENSION_TEMPLATE void Renderer::createSwapChain(Window<N> &p_Window) noexcept
{
    VkExtent2D windowExtent = {p_Window.Width(), p_Window.Height()};
    while (windowExtent.width == 0 || windowExtent.height == 0)
    {
        windowExtent = {p_Window.Width(), p_Window.Height()};
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_Device->VulkanDevice());
    m_SwapChain = KIT::Scope<SwapChain>(new SwapChain(windowExtent, p_Window.Surface(), m_SwapChain.get()));
}

void Renderer::createCommandBuffers() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Device->CommandPool();
    allocInfo.commandBufferCount = SwapChain::MAX_FRAMES_IN_FLIGHT;

    KIT_ASSERT_RETURNS(vkAllocateCommandBuffers(m_Device->VulkanDevice(), &allocInfo, m_CommandBuffers.data()),
                       VK_SUCCESS, "Failed to create command buffers");
}

template Renderer::Renderer(Window<2> &) noexcept;
template Renderer::Renderer(Window<3> &) noexcept;

template VkCommandBuffer Renderer::BeginFrame<2>(Window<2> &) noexcept;
template VkCommandBuffer Renderer::BeginFrame<3>(Window<3> &) noexcept;

template void Renderer::EndFrame<2>(Window<2> &) noexcept;
template void Renderer::EndFrame<3>(Window<3> &) noexcept;

} // namespace ONYX