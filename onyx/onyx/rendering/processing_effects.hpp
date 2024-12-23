#pragma once

#include "onyx/core/core.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "vkit/pipeline/graphics_pipeline.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"

namespace Onyx
{
class ProcessingEffect
{
  public:
    ProcessingEffect(VkRenderPass p_RenderPass, const VKit::Shader &p_VertexShader) noexcept;
    ~ProcessingEffect() noexcept;

    void Draw(VkCommandBuffer p_CommandBuffer) const noexcept;

    /**
     * @brief Allows the resource containers to be resized based on the pipeline layout.
     *
     * This method is exposed due to the deferred nature of the setup calls. It is called by the frame scheduler when
     * the setup calls are made. This is necessary because the user may try to update the resources before the actual
     * deferred setup takes place, and so the arrays must be properly resized.
     *
     * @param p_Info
     */
    void ResizeResourceContainers(const VKit::PipelineLayout::Info &p_Info) noexcept;

    // Add no index overload because the most common use case is to have only one descriptor set/push constant range

    void UpdateDescriptorSet(u32 p_Index, const PerFrameData<VkDescriptorSet> &p_DescriptorSet) noexcept;
    void UpdateDescriptorSet(const PerFrameData<VkDescriptorSet> &p_DescriptorSet) noexcept;

    template <typename T> void UpdatePushConstantRange(u32 p_Index, const T *p_Data) noexcept
    {
        m_PushData[p_Index] = {p_Data, sizeof(T)};
    }
    template <typename T> void UpdatePushConstantRange(const T *p_Data) noexcept
    {
        UpdatePushConstantRange(0, p_Data);
    }

    explicit(false) operator bool() const noexcept;

  protected:
    void bind(VkCommandBuffer p_CommandBuffer, std::span<const VkDescriptorSet> p_Sets) const noexcept;
    void setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader, u32 p_Subpass) noexcept;

    struct PushDataInfo
    {
        const void *Data = nullptr;
        u32 Size = 0;
    };
    TKit::StaticArray8<PerFrameData<VkDescriptorSet>> m_DescriptorSets{};

  private:
    VkRenderPass m_RenderPass;
    VKit::Shader m_VertexShader;
    VKit::PipelineLayout m_Layout{};
    VKit::GraphicsPipeline m_Pipeline{};
    TKit::StaticArray4<PushDataInfo> m_PushData{};
};

class PreProcessing final : public ProcessingEffect
{
    TKIT_NON_COPYABLE(PreProcessing);

  public:
    struct Specs
    {
        VKit::PipelineLayout Layout;
        VKit::Shader FragmentShader;
    };

    using ProcessingEffect::ProcessingEffect;

    /**
     * @brief Sets up the pre-processing pipeline, which is used to apply effects to the scene before the main rendering
     * pass.
     *
     * This setup call is NOT deferred, and will take effect immediately, which may cause crashes if used incorrectly.
     * The user is not expected to call this method directly, but rather through the FrameScheduler.
     *
     * If you wish to switch to a different pre-processing pipeline, call this method again with the new specifications.
     * Do not call RemovePreProcessing before or after that in the same frame, as that call will override the setup.
     *
     * @param p_Layout The pipeline layout to use for the pre-processing pipeline.
     * @param p_FragmentShader The fragment shader to use for the pre-processing pipeline.
     * @return A pointer to the pre-processing pipeline.
     */
    void Setup(const Specs &p_Specs) noexcept;
    void Bind(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer) const noexcept;
};

class PostProcessing final : public ProcessingEffect
{
    TKIT_NON_COPYABLE(PostProcessing);

  public:
    struct Specs
    {
        VKit::PipelineLayout Layout;
        VKit::Shader FragmentShader;
        VkSamplerCreateInfo SamplerCreateInfo = DefaultSamplerCreateInfo();
    };

    PostProcessing(VkRenderPass p_RenderPass, const VKit::Shader &p_VertexShader,
                   const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept;
    ~PostProcessing() noexcept;

    /**
     * @brief Creates a pipeline layout builder for the post-processing pipeline.
     *
     * Because the post processing pipeline allows the user to read from the frame's data as a sampled texture,
     * it is necessary to create a pipeline layout that includes a sampler descriptor set layout. This method
     * creates a pipeline layout builder with the necessary descriptor set layout. The user can then add any
     * additional descriptor set layouts or push constant ranges as needed.
     *
     * Failiure to use this method to create the pipeline layout will result in a runtime error when the post
     * processing pipeline is set up.
     *
     * @return A pipeline layout builder for the post-processing pipeline.
     */
    VKit::PipelineLayout::Builder CreatePipelineLayoutBuilder() const noexcept;

    /**
     * @brief Sets up the post-processing pipeline, which is used to apply effects to the scene after the main rendering
     * pass.
     *
     * This setup call is NOT deferred, and will take effect immediately, which may cause crashes if used incorrectly.
     * The user is not expected to call this method directly, but rather through the FrameScheduler.
     *
     * If you wish to switch to a different post-processing pipeline, call this method again with the new
     * specifications. Do not call RemovePostProcessing before or after that in the same frame, as that call will
     * override the setup.
     *
     * @param p_Layout The pipeline layout to use for the post-processing pipeline.
     * @param p_FragmentShader The fragment shader to use for the post-processing pipeline.
     * @param p_Info Optional sampler information to use for the post-processing pipeline.
     * @return A pointer to the post-processing pipeline.
     */
    void Setup(const Specs &p_Specs) noexcept;
    void Bind(u32 p_FrameIndex, u32 p_ImageIndex, VkCommandBuffer p_CommandBuffer) const noexcept;

    void UpdateImageViews(const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept;
    static VkSamplerCreateInfo DefaultSamplerCreateInfo() noexcept;

  private:
    void overwriteSamplerSet(VkImageView p_ImageView, VkDescriptorSet p_Set) const noexcept;

    TKit::StaticArray4<VkDescriptorSet> m_SamplerDescriptorSets;
    TKit::StaticArray4<VkImageView> m_ImageViews;
    VKit::DescriptorSetLayout m_DescriptorSetLayout{};
    VkSampler m_Sampler = VK_NULL_HANDLE;
};
} // namespace Onyx