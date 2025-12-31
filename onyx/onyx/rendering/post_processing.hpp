#pragma once

#include "onyx/core/core.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "vkit/pipeline/graphics_pipeline.hpp"
#include "vkit/pipeline/shader.hpp"
#include "vkit/pipeline/pipeline_job.hpp"

namespace Onyx
{
class ONYX_API PostProcessing
{
  public:
    struct Specs
    {
        VKit::PipelineLayout Layout;
        VKit::Shader VertexShader;
        VKit::Shader FragmentShader;
        VkSamplerCreateInfo SamplerCreateInfo = DefaultSamplerCreateInfo();
        VkPipelineRenderingCreateInfoKHR RenderInfo;
    };

    PostProcessing(const PerImageData<VkImageView> &p_ImageViews);
    ~PostProcessing();

    void UpdateDescriptorSet(const u32 p_Index, const VkDescriptorSet p_DescriptorSet)
    {
        // To account for reserved sampled image slot
        m_Job.UpdateDescriptorSet(p_Index + 1, p_DescriptorSet);
    }

    template <typename T>
    void UpdatePushConstantRange(u32 p_Index, const T *p_Data,
                                 const VkShaderStageFlags p_Stages = VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        m_Job.UpdatePushConstantRange(p_Index, p_Data, p_Stages);
    }

    /**
     * @brief Creates a pipeline layout builder for the post-processing pipeline.
     *
     * Because the post processing pipeline allows the user to read from the frame's data as a sampled texture,
     * it is necessary to create a pipeline layout that includes a sampler descriptor set layout. This method
     * creates a pipeline layout builder with the necessary descriptor set layout. The user can then add any
     * additional descriptor set layouts or push constant ranges as needed.
     *
     * Failure to use this method to create the pipeline layout will result in a runtime error when the post
     * processing pipeline is set up.
     *
     * @return A pipeline layout builder for the post-processing pipeline.
     */
    VKit::PipelineLayout::Builder CreatePipelineLayoutBuilder() const;

    /**
     * @brief Sets up the post-processing pipeline, which is used to apply effects to the scene after the main rendering
     * pass.
     *
     * If you wish to switch to a different post-processing pipeline, call this method again with the new
     * specifications. Do not call RemovePostProcessing before or after that in the same frame, as that call will
     * override the setup.
     *
     * @param p_Specs The specifications for the post-processing pipeline.
     */
    void Setup(const Specs &p_Specs);

    void Bind(VkCommandBuffer p_CommandBuffer, u32 p_ImageIndex);
    void Draw(VkCommandBuffer p_CommandBuffer) const;

    static VkSamplerCreateInfo DefaultSamplerCreateInfo();

  private:
    void updateImageViews(const PerImageData<VkImageView> &p_ImageViews);
    void overwriteSamplerSet(VkImageView p_ImageView, VkDescriptorSet p_Set) const;

    VKit::GraphicsPipeline m_Pipeline{};
    VKit::GraphicsJob m_Job{};

    PerImageData<VkImageView> m_ImageViews;
    PerImageData<VkDescriptorSet> m_SamplerDescriptors;
    VKit::DescriptorSetLayout m_DescriptorSetLayout{};
    VkSampler m_Sampler = VK_NULL_HANDLE;

    friend class FrameScheduler;
};
} // namespace Onyx
