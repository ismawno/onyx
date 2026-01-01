#include "onyx/core/pch.hpp"
#include "onyx/rendering/post_processing.hpp"
#include "onyx/resource/assets.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "vkit/state/pipeline_job.hpp"

namespace Onyx
{
PostProcessing::PostProcessing(const PerImageData<VkImageView> &p_ImageViews) : m_ImageViews(p_ImageViews)
{
    const auto result = VKit::DescriptorSetLayout::Builder(Core::GetDevice())
                            .AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .Build();
    VKIT_ASSERT_RESULT(result);
    m_DescriptorSetLayout = result.GetValue();
}
PostProcessing::~PostProcessing()
{
    Core::GetDeviceTable().DestroySampler(Core::GetDevice(), m_Sampler,
                                          nullptr); // Sampler guaranteed to exist because of frame scheduler
    m_DescriptorSetLayout.Destroy();
    m_Pipeline.Destroy();
}

VKit::PipelineLayout::Builder PostProcessing::CreatePipelineLayoutBuilder() const
{
    return VKit::PipelineLayout::Builder(Core::GetDevice()).AddDescriptorSetLayout(m_DescriptorSetLayout);
}

void PostProcessing::Setup(const Specs &p_Specs)
{
    TKIT_ASSERT(
        !p_Specs.Layout.GetInfo().DescriptorSetLayouts.IsEmpty() &&
            p_Specs.Layout.GetInfo().DescriptorSetLayouts[0] == m_DescriptorSetLayout.GetHandle(),
        "[ONYX] The pipeline layout used must be created from the PostProcessing's CreatePipelineLayoutBuilder method");

    Core::DeviceWaitIdle();
    const auto &table = Core::GetDeviceTable();
    if (m_Sampler)
        table.DestroySampler(Core::GetDevice(), m_Sampler, nullptr);

    VKIT_ASSERT_EXPRESSION(table.CreateSampler(Core::GetDevice(), &p_Specs.SamplerCreateInfo, nullptr, &m_Sampler));

    const auto result = VKit::GraphicsPipeline::Builder(Core::GetDevice(), p_Specs.Layout, p_Specs.RenderInfo)
                            .AddShaderStage(p_Specs.VertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                            .AddShaderStage(p_Specs.FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .SetViewportCount(1)
                            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                            .AddDefaultColorAttachment()
                            .Bake()
                            .Build();

    VKIT_ASSERT_RESULT(result);
    if (m_Pipeline)
        m_Pipeline.Destroy();
    m_Pipeline = result.GetValue();

    const auto jresult = VKit::GraphicsJob::Create(m_Pipeline, p_Specs.Layout);
    VKIT_ASSERT_RESULT(jresult);
    m_Job = jresult.GetValue();
    if (m_SamplerDescriptors.IsEmpty())
    {
        const VKit::DescriptorPool &pool = Assets::GetDescriptorPool();
        for (u32 i = 0; i < m_ImageViews.GetSize(); ++i)
        {
            const auto dresult = pool.Allocate(m_DescriptorSetLayout);
            VKIT_ASSERT_RESULT(dresult);
            m_SamplerDescriptors.Append(dresult.GetValue());
        }
    }

    for (u32 i = 0; i < m_ImageViews.GetSize(); ++i)
        overwriteSamplerSet(m_ImageViews[i], m_SamplerDescriptors[i]);
}

void PostProcessing::Bind(const VkCommandBuffer p_CommandBuffer, const u32 p_ImageIndex)
{
    m_Job.UpdateDescriptorSet(0, m_SamplerDescriptors[p_ImageIndex]);
    m_Job.Bind(p_CommandBuffer);
}
void PostProcessing::Draw(const VkCommandBuffer p_CommandBuffer) const
{
    m_Job.Draw(p_CommandBuffer, 3);
}

VkSamplerCreateInfo PostProcessing::DefaultSamplerCreateInfo()
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

void PostProcessing::updateImageViews(const PerImageData<VkImageView> &p_ImageViews)
{
    TKIT_ASSERT(m_ImageViews.GetSize() == p_ImageViews.GetSize(), "[ONYX] Image view count mismatch");
    m_ImageViews = p_ImageViews;
    for (u32 i = 0; i < m_ImageViews.GetSize(); ++i)
        overwriteSamplerSet(m_ImageViews[i], m_SamplerDescriptors[i]);
}

void PostProcessing::overwriteSamplerSet(const VkImageView p_ImageView, const VkDescriptorSet p_Set) const
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
