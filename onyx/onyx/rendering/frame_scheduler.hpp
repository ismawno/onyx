#pragma once
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/core/dimension.hpp"
#include "kit/multiprocessing/task.hpp"

namespace ONYX
{
struct Color;
class Window;

class ONYX_API FrameScheduler
{
  public:
    explicit FrameScheduler(Window &p_Window) noexcept;
    ~FrameScheduler() noexcept;

    VkCommandBuffer BeginFrame(Window &p_Window) noexcept;
    void EndFrame(Window &p_Window) noexcept;

    void BeginRenderPass(const Color &p_ClearColor) noexcept;
    void EndRenderPass() noexcept;

    u32 GetFrameIndex() const noexcept;

    template <typename F> void ImmediateSubmission(F &&p_Submission) const noexcept
    {
        const VkCommandBuffer cmd = m_Device->BeginSingleTimeCommands(m_CommandPool);
        std::forward<F>(p_Submission)(cmd);
        m_Device->EndSingleTimeCommands(cmd, m_CommandPool);
    }

    VkCommandPool GetCommandPool() const noexcept;

    VkCommandBuffer GetCurrentCommandBuffer() const noexcept;
    const SwapChain &GetSwapChain() const noexcept;

    VkPresentModeKHR GetPresentMode() const noexcept;
    void SetPresentMode(VkPresentModeKHR p_PresentMode) noexcept;

    const DynamicArray<VkPresentModeKHR> &GetSupportedPresentModes() const noexcept;

  private:
    void createSwapChain(Window &p_Window) noexcept;
    void createCommandPool(VkSurfaceKHR p_Surface) noexcept;
    void createCommandBuffers() noexcept;

    VkCommandPool m_CommandPool;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    DynamicArray<VkPresentModeKHR> m_SupportedPresentModes;

    KIT::Ref<Device> m_Device;
    KIT::Scope<SwapChain> m_SwapChain;
    std::array<VkCommandBuffer, SwapChain::MFIF> m_CommandBuffers;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    bool m_FrameStarted = false;
    bool m_PresentModeChanged = false;

    KIT::Ref<KIT::Task<VkResult>> m_PresentTask;
    bool m_PresentRunning = false;
};
} // namespace ONYX