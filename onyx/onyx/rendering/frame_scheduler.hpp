#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/processing_effects.hpp"
#include "vkit/rendering/swap_chain.hpp"
#include "vkit/rendering/render_pass.hpp"
#include "vkit/backend/command_pool.hpp"
#include "tkit/multiprocessing/task.hpp"

namespace Onyx
{
struct Color;
class Window;

class ONYX_API FrameScheduler
{
    TKIT_NON_COPYABLE(FrameScheduler)
  public:
    explicit FrameScheduler(Window &p_Window) noexcept;
    ~FrameScheduler() noexcept;

    VkCommandBuffer BeginFrame(Window &p_Window) noexcept;
    void EndFrame(Window &p_Window) noexcept;

    void BeginRenderPass(const Color &p_ClearColor) noexcept;
    void EndRenderPass() noexcept;

    u32 GetFrameIndex() const noexcept;

    VkResult AcquireNextImage() noexcept;
    VkResult SubmitCurrentCommandBuffer() noexcept;
    VkResult Present() noexcept;

    void RemovePreProcessing() noexcept;
    void RemovePostProcessing() noexcept;

    template <typename F> void ImmediateSubmission(F &&p_Submission) const noexcept
    {
        const auto cmdresult = m_CommandPool.BeginSingleTimeCommands();
        VKIT_ASSERT_RESULT(cmdresult);

        const VkCommandBuffer cmd = cmdresult.GetValue();
        std::forward<F>(p_Submission)(cmd);
        const auto result =
            m_CommandPool.EndSingleTimeCommands(cmd, Core::GetDevice().GetQueue(VKit::QueueType::Graphics));

        VKIT_ASSERT_VULKAN_RESULT(result);
    }

    VkRenderPass GetRenderPass() const noexcept;

    VkCommandBuffer GetCurrentCommandBuffer() const noexcept;
    const VKit::SwapChain &GetSwapChain() const noexcept;

    VkPresentModeKHR GetPresentMode() const noexcept;
    void SetPresentMode(VkPresentModeKHR p_PresentMode) noexcept;

  private:
    void createSwapChain(Window &p_Window) noexcept;
    void recreateSwapChain(Window &p_Window) noexcept;
    void createRenderPass() noexcept;
    void createCommandPool() noexcept;
    void createCommandBuffers() noexcept;

    void setupNaivePostProcessing() noexcept;
    TKit::StaticArray4<VkImageView> getIntermediateAttachmentImageViews() const noexcept;

    VKit::CommandPool m_CommandPool;
    VKit::SwapChain m_SwapChain;
    VKit::RenderPass m_RenderPass;
    VKit::RenderPass::Resources m_Resources;
    TKit::StaticArray4<VkFence> m_InFlightImages;

    TKit::Storage<PreProcessing> m_PreProcessing;
    TKit::Storage<PostProcessing> m_PostProcessing;

    VKit::Shader m_PreProcessingVertexShader;
    VKit::Shader m_PostProcessingVertexShader;
    VKit::Shader m_NaivePostProcessingFragmentShader;

    VKit::PipelineLayout m_NaivePostProcessingLayout;

    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;

    PerFrameData<VkCommandBuffer> m_CommandBuffers;
    PerFrameData<VKit::SyncData> m_SyncData{};

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    bool m_FrameStarted = false;
    bool m_PresentModeChanged = false;

    TKit::Ref<TKit::Task<VkResult>> m_PresentTask;
};
} // namespace Onyx