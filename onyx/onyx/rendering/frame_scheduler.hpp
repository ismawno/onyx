#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/processing_effects.hpp"
#include "vkit/rendering/swap_chain.hpp"
#include "vkit/rendering/render_pass.hpp"
#include "vkit/backend/command_pool.hpp"
#include "tkit/multiprocessing/task.hpp"

// Documentar y explicar bien TODO. Añadir comentario al crear render pass (pq intermediate es input etc)
// Actualizar un poco los readme

namespace Onyx
{
struct Color;
class Window;

/**
 * @brief Manages frame scheduling and rendering operations for a window.
 *
 * The `FrameScheduler` class provides a high-level abstraction for managing Vulkan rendering
 * tasks, including frame synchronization, command buffer management, and render pass execution.
 *
 * It currently provides a single render pass with support for pre- and post-processing effects, which are split into
 * multiple subpasses.
 */
class ONYX_API FrameScheduler
{
    TKIT_NON_COPYABLE(FrameScheduler)
  public:
    explicit FrameScheduler(Window &p_Window) noexcept;
    ~FrameScheduler() noexcept;

    /**
     * @brief Begins a new frame and prepares a command buffer for rendering.
     *
     * Synchronizes with the GPU to ensure the next swap chain image is ready for rendering. Will wait for the present
     * task before proceeding.
     *
     * @param p_Window The window associated with the rendering context.
     * @return A Vulkan command buffer for the current frame. May be a null handle if the swap chain needs to be
     * recreated.
     */
    VkCommandBuffer BeginFrame(Window &p_Window) noexcept;

    /**
     * @brief Finalizes the current frame and submits the rendering commands.
     *
     * Ensures all recorded commands for the frame are submitted for execution.
     *
     * @param p_Window The window associated with the rendering context.
     */
    void EndFrame(Window &p_Window) noexcept;

    /**
     * @brief Begins a render pass with the specified clear color.
     *
     * It will clear the framebuffer with the provided color and set dynamic viewport and scissor states. It will also
     * run the pre processing pipeline, if any.
     *
     * @param p_ClearColor The color to clear the framebuffer with.
     */
    void BeginRenderPass(const Color &p_ClearColor) noexcept;

    /**
     * @brief Ends the current render pass and runs the post processing pipeline.
     *
     * If not specified, the post-processing pipeline will be a naive one that simply blits the final image to the
     * swap chain image.
     *
     */
    void EndRenderPass() noexcept;

    u32 GetFrameIndex() const noexcept;

    /**
     * @brief Acquires the next image from the swap chain for rendering.
     *
     * Synchronizes rendering with the presentation engine.
     *
     * @return A Vulkan result indicating success or failure.
     */
    VkResult AcquireNextImage() noexcept;

    /**
     * @brief Submits the current command buffer for execution.
     *
     * Sends recorded commands to the GPU for processing.
     *
     * @return A Vulkan result indicating success or failure.
     */
    VkResult SubmitCurrentCommandBuffer() noexcept;

    /**
     * @brief Presents the rendered frame to the screen.
     *
     * Synchronizes frame rendering with presentation.
     *
     * @return A Vulkan result indicating success or failure.
     */
    VkResult Present() noexcept;

    PreProcessing *GetPreProcessing() noexcept;
    PostProcessing *GetPostProcessing() noexcept;

    void RemovePreProcessing() noexcept;
    void RemovePostProcessing() noexcept;

    /**
     * @brief Immediately submits a command buffer for execution.
     *
     * @param p_Submission The callable to execute on the command buffer.
     */
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
    void createProcessingEffects() noexcept;
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

    VKit::Shader m_ProcessingEffectVertexShader;
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