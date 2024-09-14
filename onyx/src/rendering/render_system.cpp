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
    return specs;
}

ONYX_DIMENSION_TEMPLATE RenderSystem::RenderSystem(const Specs<N> &p_Specs) noexcept
    : m_Pipeline(toPipelineSpecs(p_Specs))
{
}

void RenderSystem::Display(const DrawInfo &p_Info) noexcept
{
    m_Pipeline.Bind(p_Info.CommandBuffer);
    for (const DrawData &data : m_DrawData)
    {
        vkCmdPushConstants(p_Info.CommandBuffer, m_Pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstantData), &data.Data);

        // TODO: Avoid this if model is already bound
        data.Model->Bind(p_Info.CommandBuffer);
        data.Model->Draw(p_Info.CommandBuffer);
    }
}

void RenderSystem::ClearRenderData() noexcept
{
    m_DrawData.clear();
}
void RenderSystem::SubmitRenderData(const DrawData &p_Data) noexcept
{
    m_DrawData.push_back(p_Data);
}

template RenderSystem::RenderSystem(const Specs<2> &p_Specs) noexcept;
template RenderSystem::RenderSystem(const Specs<3> &p_Specs) noexcept;

} // namespace ONYX