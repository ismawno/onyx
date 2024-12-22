#include "onyx/core/pch.hpp"
#include "onyx/rendering/processing_effect.hpp"
#include "onyx/core/shaders.hpp"
#include "vkit/descriptors/descriptor_set.hpp"

namespace Onyx
{
ProcessingEffect::ProcessingEffect(VkRenderPass p_RenderPass) noexcept : m_RenderPass(p_RenderPass)
{
}
ProcessingEffect::~ProcessingEffect() noexcept
{
    m_Pipeline.Destroy();
    m_Layout.Destroy();
}

void ProcessingEffect::Setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader) noexcept
{
    m_Layout = p_Layout;

    VKit::GraphicsPipeline::Specs specs{};
    specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    specs.DepthStencilInfo.depthWriteEnable = VK_FALSE;
    specs.Layout = p_Layout;
    specs.RenderPass = m_RenderPass;

    specs.VertexShader = CreateAndCompileShader(ONYX_ROOT_PATH "/onyx/shaders/full-pass.vert");
    specs.FragmentShader = p_FragmentShader;

    const auto result = VKit::GraphicsPipeline::Create(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    m_Pipeline = result.GetValue();
}
void ProcessingEffect::UpdateDescriptorSet(u32 p_Index, const DescriptorFrameData &p_DescriptorSet) noexcept
{
    m_DescriptorSets[p_Index] = p_DescriptorSet;
}
void ProcessingEffect::UpdateDescriptorSet(const DescriptorFrameData &p_DescriptorSet) noexcept
{
    UpdateDescriptorSet(0, p_DescriptorSet);
}

void ProcessingEffect::Bind(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer,
                            const std::span<const u32> p_DynamicOffsets) const noexcept
{
    m_Pipeline.Bind(p_CommandBuffer);
    TKit::StaticArray8<VkDescriptorSet> descriptorSets;

    const u32 descriptorCount = static_cast<u32>(m_DescriptorSets.size());
    for (u32 i = 0; i < descriptorCount; ++i)
        if (m_DescriptorSets[i][p_FrameIndex])
            descriptorSets.push_back(m_DescriptorSets[i][p_FrameIndex]);

    VKit::DescriptorSet::Bind(p_CommandBuffer, descriptorSets, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Layout, 0,
                              p_DynamicOffsets);

    u32 offset = 0;
    const u32 pushCount = static_cast<u32>(m_PushData.size());
    for (u32 i = 0; i < pushCount; ++i)
    {
        const PushDataInfo &info = m_PushData[i];
        if (!info.Data)
            continue;
        vkCmdPushConstants(p_CommandBuffer, m_Layout, VK_SHADER_STAGE_COMPUTE_BIT, offset, info.Size, info.Data);
        offset += info.Size;
    }
}

} // namespace Onyx