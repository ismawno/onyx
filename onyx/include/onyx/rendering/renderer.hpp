#pragma once
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/core/dimension.hpp"
#include "kit/multiprocessing/task.hpp"

namespace ONYX
{
struct Color;
class IWindow;

class ONYX_API Renderer
{
  public:
    explicit Renderer(IWindow &p_Window) noexcept;
    ~Renderer() noexcept;

    VkCommandBuffer BeginFrame(IWindow &p_Window) noexcept;
    void EndFrame(IWindow &p_Window) noexcept;

    void BeginRenderPass(const Color &p_ClearColor) noexcept;
    void EndRenderPass() noexcept;

    template <typename F> void ImmediateSubmission(F &&p_Submission) const noexcept
    {
        const VkCommandBuffer cmd = m_Device->BeginSingleTimeCommands();
        std::forward<F>(p_Submission)(cmd);
        m_Device->EndSingleTimeCommands(cmd);
    }

    VkCommandBuffer CurrentCommandBuffer() const noexcept;
    const SwapChain &GetSwapChain() const noexcept;

  private:
    void createSwapChain(IWindow &p_Window) noexcept;
    void createCommandPool(VkSurfaceKHR p_Surface) noexcept;
    void createCommandBuffers() noexcept;

    VkCommandPool m_CommandPool;

    KIT::Ref<Device> m_Device;
    KIT::Scope<SwapChain> m_SwapChain;
    std::array<VkCommandBuffer, SwapChain::MAX_FRAMES_IN_FLIGHT> m_CommandBuffers;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    bool m_FrameStarted = false;

    KIT::Ref<KIT::Task<VkResult>> m_PresentTask;
};
} // namespace ONYX