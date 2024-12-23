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
    using ProcessingEffect::ProcessingEffect;

    void Setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader) noexcept;
    void Bind(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer) const noexcept;
};

class PostProcessing final : public ProcessingEffect
{
    TKIT_NON_COPYABLE(PostProcessing);

  public:
    PostProcessing(VkRenderPass p_RenderPass, const VKit::Shader &p_VertexShader,
                   const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept;
    ~PostProcessing() noexcept;

    VKit::PipelineLayout::Builder CreatePipelineLayoutBuilder() const noexcept;
    void Setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
               const VkSamplerCreateInfo *p_SamplerCreateInfo = nullptr) noexcept;
    void Bind(u32 p_FrameIndex, u32 p_ImageIndex, VkCommandBuffer p_CommandBuffer) const noexcept;

    void UpdateImageViews(const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept;

  private:
    void overwriteSamplerSet(VkImageView p_ImageView, VkDescriptorSet p_Set) const noexcept;

    TKit::StaticArray4<VkDescriptorSet> m_SamplerDescriptorSets;
    TKit::StaticArray4<VkImageView> m_ImageViews;
    VKit::DescriptorSetLayout m_DescriptorSetLayout{};
    VkSampler m_Sampler = VK_NULL_HANDLE;
};
} // namespace Onyx