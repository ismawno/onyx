#pragma once

#include "onyx/core/core.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "vkit/pipeline/graphics_pipeline.hpp"

namespace Onyx
{
class ProcessingEffect
{
    TKIT_NON_COPYABLE(ProcessingEffect);

  public:
    using DescriptorFrameData = std::array<VkDescriptorSet, ONYX_MAX_FRAMES_IN_FLIGHT>;

    ProcessingEffect(VkRenderPass p_RenderPass) noexcept;
    ~ProcessingEffect() noexcept;

    void Setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader) noexcept;

    // Add no index overload because the most common use case is to have only one descriptor set/push constant range

    void UpdateDescriptorSet(u32 p_Index, const DescriptorFrameData &p_DescriptorSet) noexcept;
    void UpdateDescriptorSet(const DescriptorFrameData &p_DescriptorSet) noexcept;

    template <typename T> void UpdatePushConstantRange(u32 p_Index, const T *p_Data) noexcept
    {
        m_PushData[p_Index] = {p_Data, sizeof(T)};
    }
    template <typename T> void UpdatePushConstantRange(const T *p_Data) noexcept
    {
        UpdatePushConstantRange(0, p_Data);
    }

    void Bind(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer,
              std::span<const u32> p_DynamicOffsets = {}) const noexcept;
    void Draw(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer) const noexcept;

  private:
    struct PushDataInfo
    {
        const void *Data = nullptr;
        u32 Size = 0;
    };

    VkRenderPass m_RenderPass;
    VKit::PipelineLayout m_Layout{};
    VKit::GraphicsPipeline m_Pipeline{};
    TKit::StaticArray8<DescriptorFrameData> m_DescriptorSets{};
    TKit::StaticArray4<PushDataInfo> m_PushData{};
};
} // namespace Onyx