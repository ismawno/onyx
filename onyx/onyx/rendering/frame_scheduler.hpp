#pragma once

#include "onyx/execution/sync.hpp"
#include "vkit/presentation/swap_chain.hpp"
#include "vkit/execution/command_pool.hpp"
#include "vkit/resource/image.hpp"

namespace Onyx
{
struct Color;
class Window;

enum TransferMode : u8
{
    Transfer_Separate = 0,
    Transfer_SameIndex = 1,
    Transfer_SameQueue = 2
};

struct WaitMode
{
    u64 WaitFenceTimeout;
    u64 AcquireTimeout;

    static const WaitMode Block;
    static const WaitMode Poll;
};

class FrameScheduler
{
    TKIT_NON_COPYABLE(FrameScheduler)
  public:
    struct ImageData
    {
        VKit::DeviceImage *Presentation;
        VKit::DeviceImage DepthStencil;
    };
    struct CommandData
    {
        VKit::CommandPool GraphicsPool;
        VKit::CommandPool TransferPool;
        VkCommandBuffer GraphicsCommand;
        VkCommandBuffer TransferCommand;
    };

    FrameScheduler(Window &p_Window);
    ~FrameScheduler();

    VkCommandBuffer BeginFrame(Window &p_Window, const WaitMode &p_WaitMode);

    void EndFrame(Window &p_Window, VkPipelineStageFlags p_Flags);
    void BeginRendering(const Color &p_ClearColor);

    void EndRendering();

    u32 GetFrameIndex() const
    {
        return m_FrameIndex;
    }

    VkPipelineRenderingCreateInfoKHR CreateSceneRenderInfo() const;

    VkResult AcquireNextImage(const WaitMode &p_WaitMode);

    void SubmitGraphicsQueue(VkPipelineStageFlags p_Flags);
    void SubmitTransferQueue();
    VkResult Present();

    VkCommandBuffer GetGraphicsCommandBuffer() const
    {
        return m_CommandData[m_FrameIndex].GraphicsCommand;
    }
    VkCommandBuffer GetTransferCommandBuffer() const
    {
        return m_CommandData[m_FrameIndex].TransferCommand;
    }

    VKit::Queue *GetGraphicsQueue() const
    {
        return m_Graphics;
    }

    const VKit::SwapChain &GetSwapChain() const
    {
        return m_SwapChain;
    }
    VkPresentModeKHR GetPresentMode() const
    {
        return m_PresentMode;
    }
    const TKit::Array8<VkPresentModeKHR> &GetAvailablePresentModes() const
    {
        return m_SwapChain.GetInfo().SupportDetails.PresentModes;
    }

    void SetPresentMode(const VkPresentModeKHR p_PresentMode)
    {
        if (m_PresentMode == p_PresentMode)
            return;
        m_RequestSwapchainRecreation = true;
        m_PresentMode = p_PresentMode;
    }
    void RequestSwapchainRecreation()
    {
        m_RequestSwapchainRecreation = true;
    }

  private:
    void createSwapChain(Window &p_Window, const VkExtent2D &p_WindowExtent);
    void recreateSwapChain(Window &p_Window);
    void recreateSurface(Window &p_Window);
    void recreateResources();
    void createCommandData();

    bool handleImageResult(Window &p_Window, VkResult p_Result);

    PerImageData<ImageData> createImageData();
    void destroyImageData();

    VkExtent2D waitGlfwEvents(Window &p_Window);
    PerImageData<ImageData> m_Images{};

    VKit::SwapChain m_SwapChain;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;

    PerFrameData<CommandData> m_CommandData;

    PerFrameData<Detail::SyncFrameData> m_SyncFrameData{};
    PerImageData<Detail::SyncImageData> m_SyncImageData{};

    VKit::Queue *m_Graphics;
    VKit::Queue *m_Transfer;
    VKit::Queue *m_Present;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    TransferMode m_TransferMode;
    bool m_RequestSwapchainRecreation = false;
};
} // namespace Onyx
