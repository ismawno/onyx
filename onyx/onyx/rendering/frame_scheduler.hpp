#pragma once

#include "onyx/rendering/post_processing.hpp"
#include "vkit/rendering/swap_chain.hpp"
#include "vkit/rendering/command_pool.hpp"
#include "vkit/rendering/image.hpp"

namespace Onyx
{
struct Color;
class Window;
/**
 * @brief Manages frame scheduling and rendering operations for a window.
 *
 * The `FrameScheduler` class provides a high-level abstraction for managing Vulkan rendering
 * tasks, including frame synchronization, command buffer management, and rendering execution.
 *
 */
class ONYX_API FrameScheduler
{
    TKIT_NON_COPYABLE(FrameScheduler)
  public:
    struct Image
    {
        VKit::Image Image;
        VkImageLayout Layout;
    };
    struct ImageData
    {
        Image Presentation;
        Image Intermediate;
        Image DepthStencil;
    };

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
     */
    void EndFrame(Window &p_Window) noexcept;

    /**
     * @brief Begins the main scene rendering with the specified clear color.
     *
     * It will clear the color attachment with the provided color and set dynamic viewport and scissor states.
     *
     * @param p_ClearColor The color to clear the framebuffer with.
     */
    void BeginRendering(const Color &p_ClearColor) noexcept;

    /**
     * @brief Ends the current rendering and runs the post processing pipeline.
     *
     * If not specified, the post-processing pipeline will be a naive one that simply blits the final image to the
     * swap chain image.
     *
     */
    void EndRendering() noexcept;

    u32 GetFrameIndex() const noexcept;

    /**
     * @brief Creates information needed by pipelines that wish to render to the main scene.
     *
     */
    VkPipelineRenderingCreateInfoKHR CreateSceneRenderInfo() const noexcept;

    /**
     * @brief Creates information needed by pipelines that wish to act in post processing.
     *
     */
    VkPipelineRenderingCreateInfoKHR CreatePostProcessingRenderInfo() const noexcept;

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
    VkResult SubmitCurrentCommandBuffer(u32 p_FrameIndex, u32 p_ImageIndex) noexcept;

    /**
     * @brief Presents the rendered frame to the screen.
     *
     * Synchronizes frame rendering with presentation.
     *
     * @return A Vulkan result indicating success or failure.
     */
    VkResult Present(u32 p_FrameIndex, u32 p_ImageIndex) noexcept;

    struct PostProcessingOptions
    {
        const VKit::Shader *VertexShader = nullptr;
        const VkSamplerCreateInfo *SamplerCreateInfo = nullptr;
    };

    /**
     * @brief Sets up the post-processing pipeline, which is used to apply effects to the scene after the main rendering
     * pass.
     *
     * If you wish to switch to a different post-processing pipeline, call this method again with the new
     * specifications. Do not call `RemovePostProcessing()` before or after that in the same frame, as that call will
     * override the setup.
     *
     * @param p_Layout The pipeline layout to use for the post-processing pipeline.
     * @param p_FragmentShader The fragment shader to use for the post-processing pipeline.
     *
     * The following is encoded in the `PostProcessingOptions` struct:
     *
     * @param p_VertexShader Optional vertex shader to use for the post-processing pipeline.
     * @param p_Info Optional sampler information to use for the post-processing pipeline.
     * @return A pointer to the post-processing pipeline.
     */
    PostProcessing *SetPostProcessing(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
                                      const PostProcessingOptions &p_Options = {nullptr, nullptr}) noexcept;

    PostProcessing *GetPostProcessing() noexcept;

    /**
     * @brief Block calling threads until all queue submissions are complete.
     *
     * This method only waits for `VkQueueSubmit()` calls to complete, not the underlying tasks submitted. For that, you
     * must wait on the corresponding queue to be idle.
     *
     * If the `Window::Flag_ConcurrentQueueSubmission` was not set on window creation, this is a no-op.
     *
     */
    void WaitIdle() const noexcept;

    /**
     * @brief Removes the post-processing pipeline and substitutes it with a naive one that simply blits the final
     * image.
     */
    void RemovePostProcessing() noexcept;

    const VKit::SwapChain &GetSwapChain() const noexcept;

    VkCommandBuffer GetCurrentCommandBuffer() const noexcept;

    VkPresentModeKHR GetPresentMode() const noexcept;
    void SetPresentMode(VkPresentModeKHR p_PresentMode) noexcept;

    TKit::StaticArray8<VkPresentModeKHR> GetAvailablePresentModes() const noexcept;

  private:
    void createSwapChain(Window &p_Window) noexcept;
    void recreateSwapChain(Window &p_Window) noexcept;
    void recreateSurface(Window &p_Window) noexcept;
    void recreateResources() noexcept;
    void createProcessingEffects() noexcept;
    void createCommandData() noexcept;

    void handlePresentResult(Window &p_Window, VkResult p_Result) noexcept;

    TKit::StaticArray4<ImageData> createImageData() noexcept;
    void destroyImageData() noexcept;

    void setupNaivePostProcessing() noexcept;

    TKit::StaticArray4<ImageData> m_Images{};
    TKit::StaticArray4<VkImageView> getIntermediateColorImageViews() const noexcept;

    VKit::SwapChain m_SwapChain;
    TKit::StaticArray4<VkFence> m_InFlightImages;
    TKit::Storage<PostProcessing> m_PostProcessing;

    VKit::Shader m_NaivePostProcessingFragmentShader;

    VKit::PipelineLayout m_NaivePostProcessingLayout;
    VKit::ImageHouse m_ImageHouse;

    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;

    PerFrameData<VKit::CommandPool> m_CommandPools;
    PerFrameData<VkCommandBuffer> m_CommandBuffers;
    PerFrameData<VKit::SyncData> m_SyncData{};

    TKit::Task<VkResult> *m_PresentTask = nullptr;

    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    bool m_FrameStarted = false;
    bool m_PresentModeChanged = false;
};
} // namespace Onyx
