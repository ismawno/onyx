#pragma once

#include "onyx/rendering/swap_chain.hpp"
#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Window;

class Renderer
{
  public:
    ONYX_DIMENSION_TEMPLATE explicit Renderer(Window<N> &p_Window) noexcept;
    ~Renderer() noexcept;

    ONYX_DIMENSION_TEMPLATE VkCommandBuffer BeginFrame(Window<N> &p_Window) noexcept;
    ONYX_DIMENSION_TEMPLATE void EndFrame(Window<N> &p_Window) noexcept;

  private:
    ONYX_DIMENSION_TEMPLATE void createSwapChain(Window<N> &p_Window) noexcept;
    void createCommandBuffers() noexcept;

    KIT::Ref<Device> m_Device;
    KIT::Scope<SwapChain> m_SwapChain;
    std::array<VkCommandBuffer, SwapChain::MAX_FRAMES_IN_FLIGHT> m_CommandBuffers;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    bool m_FrameStarted = false;
};
} // namespace ONYX