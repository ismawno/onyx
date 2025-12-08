#pragma once

#include "onyx/rendering/post_processing.hpp"
#include "onyx/data/sync.hpp"
#include "vkit/rendering/swap_chain.hpp"
#include "vkit/rendering/command_pool.hpp"
#include "vkit/rendering/image.hpp"

namespace Onyx
{
struct Color;
class Window;

enum class TransferMode : u8
{
    Separate = 0,
    SameIndex = 1,
    SameQueue = 2
};

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
    struct CommandData
    {
        VKit::CommandPool GraphicsPool;
        VKit::CommandPool TransferPool;
        VkCommandBuffer GraphicsCommand;
        VkCommandBuffer TransferCommand;
    };

    FrameScheduler(Window &p_Window);
    ~FrameScheduler();

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
    VkCommandBuffer BeginFrame(Window &p_Window);

    /**
     * @brief Finalizes the current frame and submits the rendering commands.
     *
     * Ensures all recorded commands for the frame are submitted for execution.
     *
     */
    void EndFrame(Window &p_Window, VkPipelineStageFlags p_Flags);

    /**
     * @brief Begins the main scene rendering with the specified clear color.
     *
     * It will clear the color attachment with the provided color and set dynamic viewport and scissor states.
     *
     * @param p_ClearColor The color to clear the framebuffer with.
     */
    void BeginRendering(const Color &p_ClearColor);

    /**
     * @brief Ends the current rendering and runs the post processing pipeline.
     *
     * If not specified, the post-processing pipeline will be a naive one that simply blits the final image to the
     * swap chain image.
     *
     */
    void EndRendering();

    u32 GetFrameIndex() const
    {
        return m_FrameIndex;
    }

    /**
     * @brief Creates information needed by pipelines that wish to render to the main scene.
     *
     */
    VkPipelineRenderingCreateInfoKHR CreateSceneRenderInfo() const;

    /**
     * @brief Creates information needed by pipelines that wish to act in post processing.
     *
     */
    VkPipelineRenderingCreateInfoKHR CreatePostProcessingRenderInfo() const;

    /**
     * @brief Acquires the next image from the swap chain for rendering.
     *
     * Synchronizes rendering with the presentation engine.
     *
     * @return A Vulkan result indicating success or failure.
     */
    VkResult AcquireNextImage();

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
     * The following is specified through the `PostProcessingOptions` struct:
     *
     * @param p_VertexShader Optional vertex shader to use for the post-processing pipeline.
     * @param p_Info Optional sampler information to use for the post-processing pipeline.
     * @return A pointer to the post-processing pipeline.
     */
    PostProcessing *SetPostProcessing(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
                                      const PostProcessingOptions &p_Options = {nullptr, nullptr});

    PostProcessing *GetPostProcessing();

    /**
     * @brief Removes the post-processing pipeline and substitutes it with a naive one that simply blits the final
     * image.
     */
    void RemovePostProcessing();

    const Detail::QueueData &GetQueueData() const
    {
        return m_QueueData;
    }
    const VKit::SwapChain &GetSwapChain() const
    {
        return m_SwapChain;
    }
    VkPresentModeKHR GetPresentMode() const
    {
        return m_PresentMode;
    }
    const TKit::StaticArray8<VkPresentModeKHR> &GetAvailablePresentModes() const
    {
        return m_SwapChain.GetInfo().SupportDetails.PresentModes;
    }

    void SetPresentMode(VkPresentModeKHR p_PresentMode);

  private:
    void createSwapChain(Window &p_Window, const VkExtent2D &p_WindowExtent);
    void recreateSwapChain(Window &p_Window);
    void recreateSurface(Window &p_Window);
    void recreateResources();
    void createProcessingEffects();
    void createCommandData();

    void handlePresentResult(Window &p_Window, VkResult p_Result);

    PerImageData<ImageData> createImageData();
    void destroyImageData();

    void setupNaivePostProcessing();
    VkExtent2D waitGlfwEvents(Window &p_Window);
    PerImageData<VkImageView> getIntermediateColorImageViews() const;

    PerImageData<ImageData> m_Images{};

    VKit::SwapChain m_SwapChain;
    TKit::Storage<PostProcessing> m_PostProcessing;

    VKit::Shader m_NaivePostProcessingFragmentShader;

    VKit::PipelineLayout m_NaivePostProcessingLayout;
    VKit::ImageHouse m_ImageHouse;

    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;

    PerFrameData<CommandData> m_CommandData;

    PerFrameData<Detail::SyncFrameData> m_SyncFrameData{};
    PerImageData<Detail::SyncImageData> m_SyncImageData{};

    Detail::QueueData m_QueueData;
    u32 m_ImageIndex;
    u32 m_FrameIndex = 0;
    TransferMode m_TransferMode;
    bool m_FrameStarted = false;
    bool m_PresentModeChanged = false;
};
} // namespace Onyx
