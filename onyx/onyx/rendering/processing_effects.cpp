#include "onyx/core/pch.hpp"
#include "onyx/rendering/processing_effects.hpp"
#include "onyx/core/shaders.hpp"
#include "vkit/descriptors/descriptor_set.hpp"

namespace Onyx
{
ProcessingEffect::ProcessingEffect(VkRenderPass p_RenderPass, const VKit::Shader &p_VertexShader) noexcept
    : m_RenderPass(p_RenderPass), m_VertexShader(p_VertexShader)
{
}

ProcessingEffect::~ProcessingEffect() noexcept
{
    Core::DeviceWaitIdle();
    if (m_Pipeline)
        m_Pipeline.Destroy();
}

void ProcessingEffect::setup(const VKit::PipelineLayout &p_Layout, const VKit::Shader &p_FragmentShader,
                             const u32 p_Subpass) noexcept
{
    m_Layout = p_Layout;

    VKit::GraphicsPipeline::Specs specs{};
    specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    specs.DepthStencilInfo.depthWriteEnable = VK_FALSE;
    specs.Layout = p_Layout;
    specs.RenderPass = m_RenderPass;
    specs.Subpass = p_Subpass;

    specs.VertexShader = m_VertexShader;
    specs.FragmentShader = p_FragmentShader;

    if (m_Pipeline)
        m_Pipeline.Destroy();

    const auto result = VKit::GraphicsPipeline::Create(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    m_Pipeline = result.GetValue();

    m_PushData.clear();
    m_DescriptorSets.clear();

    m_PushData.resize(p_Layout.GetInfo().PushConstantRanges.size());
    m_DescriptorSets.resize(p_Layout.GetInfo().DescriptorSetLayouts.size());
}

void ProcessingEffect::bind(const VkCommandBuffer p_CommandBuffer,
                            const std::span<const VkDescriptorSet> p_Sets) const noexcept
{
    m_Pipeline.Bind(p_CommandBuffer);
    if (!p_Sets.empty())
        VKit::DescriptorSet::Bind(p_CommandBuffer, p_Sets, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Layout);

    u32 offset = 0;
    const u32 pushCount = static_cast<u32>(m_PushData.size());
    for (u32 i = 0; i < pushCount; ++i)
    {
        const PushDataInfo &info = m_PushData[i];
        if (!info.Data)
            continue;
        vkCmdPushConstants(p_CommandBuffer, m_Layout, VK_SHADER_STAGE_FRAGMENT_BIT, offset, info.Size, info.Data);
        offset += info.Size;
    }
}

void ProcessingEffect::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    vkCmdDraw(p_CommandBuffer, 3, 1, 0, 0);
}
void ProcessingEffect::ResizeResourceContainers(const VKit::PipelineLayout::Info &p_Info) noexcept
{
    m_PushData.resize(p_Info.PushConstantRanges.size());
    m_DescriptorSets.resize(p_Info.DescriptorSetLayouts.size());
}

void ProcessingEffect::UpdateDescriptorSet(const u32 p_Index,
                                           const PerFrameData<VkDescriptorSet> &p_DescriptorSet) noexcept
{
    m_DescriptorSets[p_Index] = p_DescriptorSet;
}
void ProcessingEffect::UpdateDescriptorSet(const PerFrameData<VkDescriptorSet> &p_DescriptorSet) noexcept
{
    UpdateDescriptorSet(0, p_DescriptorSet);
}

ProcessingEffect::operator bool() const noexcept
{
    return m_Pipeline;
}

void PreProcessing::Setup(const Specs &p_Specs) noexcept
{
    Core::DeviceWaitIdle();
    setup(p_Specs.Layout, p_Specs.FragmentShader, 0);
}
void PreProcessing::Bind(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) const noexcept
{
    TKit::StaticArray8<VkDescriptorSet> descriptorSets;
    const u32 descriptorCount = static_cast<u32>(m_DescriptorSets.size());
    for (u32 i = 0; i < descriptorCount; ++i)
        if (m_DescriptorSets[i][p_FrameIndex])
            descriptorSets.push_back(m_DescriptorSets[i][p_FrameIndex]);

    bind(p_CommandBuffer, descriptorSets);
}

PostProcessing::PostProcessing(VkRenderPass p_RenderPass, const VKit::Shader &p_VertexShader,
                               const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept
    : ProcessingEffect(p_RenderPass, p_VertexShader), m_ImageViews(p_ImageViews)
{
    const auto result = VKit::DescriptorSetLayout::Builder(Core::GetDevice())
                            .AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .Build();
    VKIT_ASSERT_RESULT(result);
    m_DescriptorSetLayout = result.GetValue();
}
PostProcessing::~PostProcessing() noexcept
{
    vkDestroySampler(Core::GetDevice(), m_Sampler, nullptr);
    m_DescriptorSetLayout.Destroy();
}

VKit::PipelineLayout::Builder PostProcessing::CreatePipelineLayoutBuilder() const noexcept
{
    return VKit::PipelineLayout::Builder(Core::GetDevice()).AddDescriptorSetLayout(m_DescriptorSetLayout);
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

void PostProcessing::Setup(const Specs &p_Specs) noexcept
{
    TKIT_ASSERT(
        p_Specs.Layout.GetInfo().DescriptorSetLayouts.empty() ||
            p_Specs.Layout.GetInfo().DescriptorSetLayouts[0] == m_DescriptorSetLayout.GetLayout(),
        "The pipeline layout used must be created from the PostProcessing's CreatePipelineLayoutBuilder method");

    Core::DeviceWaitIdle();
    if (m_Sampler)
        vkDestroySampler(Core::GetDevice(), m_Sampler, nullptr);

    TKIT_ASSERT_RETURNS(vkCreateSampler(Core::GetDevice(), &p_Specs.SamplerCreateInfo, nullptr, &m_Sampler), VK_SUCCESS,
                        "Failed to create sampler");

    m_SamplerDescriptorSets.clear();

    const VKit::DescriptorPool &pool = Core::GetDescriptorPool();
    const u32 imageCount = static_cast<u32>(m_ImageViews.size());
    for (u32 i = 0; i < imageCount; ++i)
    {
        const auto result = pool.Allocate(m_DescriptorSetLayout);
        VKIT_ASSERT_RESULT(result);
        const VkDescriptorSet set = result.GetValue();
        overwriteSamplerSet(m_ImageViews[i], set);
        m_SamplerDescriptorSets.push_back(set);
    }

    setup(p_Specs.Layout, p_Specs.FragmentShader, 2);
}

void PostProcessing::Bind(const u32 p_FrameIndex, const u32 p_ImageIndex,
                          const VkCommandBuffer p_CommandBuffer) const noexcept
{
    TKit::StaticArray8<VkDescriptorSet> descriptorSets;
    descriptorSets.push_back(m_SamplerDescriptorSets[p_ImageIndex]);

    const u32 descriptorCount = static_cast<u32>(m_DescriptorSets.size());
    for (u32 i = 0; i < descriptorCount; ++i)
        if (m_DescriptorSets[i][p_FrameIndex])
            descriptorSets.push_back(m_DescriptorSets[i][p_FrameIndex]);

    bind(p_CommandBuffer, descriptorSets);
}

void PostProcessing::UpdateImageViews(const TKit::StaticArray4<VkImageView> &p_ImageViews) noexcept
{
    TKIT_ASSERT(m_ImageViews.size() == p_ImageViews.size(), "Image view count mismatch");
    m_ImageViews = p_ImageViews;
    const u32 imageCount = static_cast<u32>(m_ImageViews.size());
    for (u32 i = 0; i < imageCount; ++i)
        overwriteSamplerSet(m_ImageViews[i], m_SamplerDescriptorSets[i]);
}

} // namespace Onyx