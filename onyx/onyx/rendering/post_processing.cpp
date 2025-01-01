#include "onyx/core/pch.hpp"
#include "onyx/rendering/post_processing.hpp"
#include "onyx/core/shaders.hpp"
#include "vkit/descriptors/descriptor_set.hpp"

namespace Onyx
{
PostProcessing::PostProcessing(const VkRenderPass p_RenderPass,
                               const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept
    : m_RenderPass(p_RenderPass), m_ImageViews(p_ImageViews)
{
    const auto result = VKit::DescriptorSetLayout::Builder(Core::GetDevice())
                            .AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .Build();
    VKIT_ASSERT_RESULT(result);
    m_DescriptorSetLayout = result.GetValue();
}
PostProcessing::~PostProcessing() noexcept
{
    vkDestroySampler(Core::GetDevice(), m_Sampler, nullptr); // Sampler guaranteed to exist because of frame scheduler
    m_DescriptorSetLayout.Destroy();
    m_Pipeline.Destroy();
}

void PostProcessing::UpdateDescriptorSet(const u32 p_Index, const VkDescriptorSet p_DescriptorSet) noexcept
{
    // To account for reserved sampled image slot
    m_Job.UpdateDescriptorSet(p_Index + 1, p_DescriptorSet);
}

VKit::PipelineLayout::Builder PostProcessing::CreatePipelineLayoutBuilder() const noexcept
{
    return VKit::PipelineLayout::Builder(Core::GetDevice()).AddDescriptorSetLayout(m_DescriptorSetLayout);
}

void PostProcessing::Setup(const Specs &p_Specs) noexcept
{
    TKIT_ASSERT(
        !p_Specs.Layout.GetInfo().DescriptorSetLayouts.empty() &&
            p_Specs.Layout.GetInfo().DescriptorSetLayouts[0] == m_DescriptorSetLayout.GetLayout(),
        "[ONYX] The pipeline layout used must be created from the PostProcessing's CreatePipelineLayoutBuilder method");

    Core::DeviceWaitIdle();
    if (m_Sampler)
        vkDestroySampler(Core::GetDevice(), m_Sampler, nullptr);

    TKIT_ASSERT_RETURNS(vkCreateSampler(Core::GetDevice(), &p_Specs.SamplerCreateInfo, nullptr, &m_Sampler), VK_SUCCESS,
                        "[ONYX] Failed to create sampler");

    const auto result = VKit::GraphicsPipeline::Builder(Core::GetDevice(), p_Specs.Layout, m_RenderPass, 1)
                            .AddShaderStage(p_Specs.VertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                            .AddShaderStage(p_Specs.FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .SetViewportCount(1)
                            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                            .AddDefaultColorAttachment()
                            .Build();

    VKIT_ASSERT_RESULT(result);
    if (m_Pipeline)
        m_Pipeline.Destroy();
    m_Pipeline = result.GetValue();

    m_Job = VKit::GraphicsJob(m_Pipeline, p_Specs.Layout);
    if (m_SamplerDescriptors.empty())
    {
        const VKit::DescriptorPool &pool = Core::GetDescriptorPool();
        for (usize i = 0; i < m_ImageViews.size(); ++i)
        {
            const auto dresult = pool.Allocate(m_DescriptorSetLayout);
            VKIT_ASSERT_RESULT(dresult);
            m_SamplerDescriptors.push_back(dresult.GetValue());
        }
    }

    for (usize i = 0; i < m_ImageViews.size(); ++i)
        overwriteSamplerSet(m_ImageViews[i], m_SamplerDescriptors[i]);
}

void PostProcessing::Bind(const VkCommandBuffer p_CommandBuffer, const u32 p_ImageIndex) noexcept
{
    m_Job.UpdateDescriptorSet(0, m_SamplerDescriptors[p_ImageIndex]);
    m_Job.Bind(p_CommandBuffer);
}
void PostProcessing::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    m_Job.Draw(p_CommandBuffer, 3);
}

VkSamplerCreateInfo PostProcessing::DefaultSamplerCreateInfo() noexcept
{
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    return samplerCreateInfo;
}

void PostProcessing::updateImageViews(const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept
{
    TKIT_ASSERT(m_ImageViews.size() == p_ImageViews.size(), "[ONYX] Image view count mismatch");
    m_ImageViews = p_ImageViews;
    const u32 imageCount = static_cast<u32>(m_ImageViews.size());
    for (u32 i = 0; i < imageCount; ++i)
        overwriteSamplerSet(m_ImageViews[i], m_SamplerDescriptors[i]);
}

void PostProcessing::overwriteSamplerSet(const VkImageView p_ImageView, const VkDescriptorSet p_Set) const noexcept
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &m_DescriptorSetLayout};
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = p_ImageView;
    info.sampler = m_Sampler;
    writer.WriteImage(0, info);
    writer.Overwrite(p_Set);
}
} // namespace Onyx
