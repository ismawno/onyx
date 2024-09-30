#include "core/pch.hpp"
#include "onyx/rendering/render_system.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE static Pipeline::Specs toPipelineSpecs(const RenderSystem::Specs<N> &p_Specs) noexcept
{
    Pipeline::Specs specs{};
    specs.VertexShaderPath = p_Specs.VertexShaderPath;
    specs.FragmentShaderPath = p_Specs.FragmentShaderPath;

    specs.PushConstantRange.size = sizeof(RenderSystem::PushConstantData);
    specs.BindingDescriptions.insert(specs.BindingDescriptions.end(), p_Specs.BindingDescriptions.begin(),
                                     p_Specs.BindingDescriptions.end());
    specs.AttributeDescriptions.insert(specs.AttributeDescriptions.end(), p_Specs.AttributeDescriptions.begin(),
                                       p_Specs.AttributeDescriptions.end());

    specs.InputAssemblyInfo.topology = p_Specs.Topology;
    specs.RasterizationInfo.polygonMode = p_Specs.PolygonMode;
    specs.RenderPass = p_Specs.RenderPass;

    if (p_Specs.Topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
        p_Specs.Topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        specs.InputAssemblyInfo.primitiveRestartEnable = VK_TRUE;

    // WARNING!! This info points to a 'unstable' memory location. Fortunately, p_Specs still lives when the pipeline is
    // constructed (see Pipeline ctor below)
    specs.PipelineLayoutInfo.pSetLayouts = p_Specs.DescriptorSetLayout ? &p_Specs.DescriptorSetLayout : nullptr;
    specs.PipelineLayoutInfo.setLayoutCount = p_Specs.DescriptorSetLayout ? 1 : 0;
    if constexpr (N == 2)
    {
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
        specs.DepthStencilInfo.depthWriteEnable = VK_FALSE;
        specs.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    }
    else
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    return specs;
}

ONYX_DIMENSION_TEMPLATE RenderSystem::RenderSystem(const Specs<N> &p_Specs) noexcept
    : m_Pipeline(toPipelineSpecs(p_Specs)), m_Is3D(N == 3)
{
}

void RenderSystem::Display(const DrawInfo &p_Info) const noexcept
{
    if (m_DrawData.empty())
        return;
    m_Pipeline.Bind(p_Info.CommandBuffer);
    vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline.GetLayout(), 0, 1,
                            &p_Info.DescriptorSet, 0, nullptr);

    for (const DrawData &data : m_DrawData)
    {
        KIT_ASSERT(data.Model,
                   "Model cannot be NULL! No drawables can be created before the creation of the first window");
        PushConstantData pushConstantData{};
        pushConstantData.ModelTransform = data.ModelTransform;
        if (m_Is3D)
            pushConstantData.ColorAndNormalMatrix = mat4(glm::transpose(mat3(glm::inverse(data.ModelTransform))));

        pushConstantData.ColorAndNormalMatrix[3] = data.Color;
        vkCmdPushConstants(p_Info.CommandBuffer, m_Pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstantData), &pushConstantData);

        if (m_BoundModel != data.Model)
        {
            data.Model->Bind(p_Info.CommandBuffer);
            m_BoundModel = data.Model;
        }
        data.Model->Draw(p_Info.CommandBuffer);
    }
}

void RenderSystem::SubmitRenderData(const RenderSystem &p_RenderSystem) noexcept
{
    m_DrawData.insert(m_DrawData.end(), p_RenderSystem.m_DrawData.begin(), p_RenderSystem.m_DrawData.end());
}
void RenderSystem::SubmitRenderData(const DrawData &p_Data) noexcept
{
    m_DrawData.push_back(p_Data);
}

void RenderSystem::ClearRenderData() noexcept
{
    m_DrawData.clear();
    m_BoundModel = nullptr;
}

template RenderSystem::RenderSystem(const Specs<2> &p_Specs) noexcept;
template RenderSystem::RenderSystem(const Specs<3> &p_Specs) noexcept;

} // namespace ONYX