#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct MeshPushConstantData;

template <> struct MeshPushConstantData<2>
{
    mat4 Transform;
    vec4 Color;
};

template <> struct MeshPushConstantData<3>
{
    mat4 Transform;
    mat4 ColorAndNormalMatrix;
};

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::MeshRenderer(const VkRenderPass p_RenderPass,
                                                      const VkDescriptorSetLayout p_Layout) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 2)
    {
        KIT_ASSERT(!p_Layout, "The 2D renderer does not require a descriptor set layout");
        specs.VertexShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.vert.spv";
        specs.FragmentShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";
    }
    else
    {
        KIT_ASSERT(p_Layout, "The 3D renderer requires a descriptor set layout");
        specs.VertexShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.vert.spv";
        specs.FragmentShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.frag.spv";
    }
    specs.PushConstantRange.size = sizeof(MeshPushConstantData<N>);
    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions.insert(specs.BindingDescriptions.end(), bdesc.begin(), bdesc.end());
    specs.AttributeDescriptions.insert(specs.AttributeDescriptions.end(), attdesc.begin(), attdesc.end());

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    // WARNING!! This info points to a 'unstable' memory location. Fortunately, p_Layout still lives when the pipeline
    // is constructed (see Pipeline ctor below)
    specs.PipelineLayoutInfo.pSetLayouts = p_Layout ? &p_Layout : nullptr;
    specs.PipelineLayoutInfo.setLayoutCount = p_Layout ? 1 : 0;
    if constexpr (N == 2)
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    else
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    m_Pipeline.Create(specs);
}

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::~MeshRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Draw(const Model *p_Model, const mat4 &p_ModelTransform,
                                                   const vec4 &p_Color) noexcept
{
    m_DrawData.emplace_back(p_Model, p_ModelTransform, p_Color);
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_DrawData.empty())
        return;

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1,
                                &p_Info.GlobalDescriptorSet, 0, nullptr);

    for (const DrawData &data : m_DrawData)
    {
        KIT_ASSERT(data.Model,
                   "Model cannot be NULL! No drawables can be created before the creation of the first window");

        MeshPushConstantData<N> pushConstantData{};
        if constexpr (N == 3)
        {
            pushConstantData.ColorAndNormalMatrix = mat4(glm::transpose(mat3(glm::inverse(data.Transform))));
            pushConstantData.ColorAndNormalMatrix[3] = data.Color;
        }
        else
            pushConstantData.Color = data.Color;

        vkCmdPushConstants(p_Info.CommandBuffer, m_Pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(MeshPushConstantData<N>), &pushConstantData);
        data.Model->Bind(p_Info.CommandBuffer);
        data.Model->Draw(p_Info.CommandBuffer);
    }
    m_DrawData.clear();
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Flush() noexcept
{
    m_DrawData.clear();
}

template class MeshRenderer<2>;
template class MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::CircleRenderer(const VkRenderPass p_RenderPass,
                                                          const VkDescriptorSetLayout p_Layout) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 2)
    {
        KIT_ASSERT(!p_Layout, "The 2D renderer does not require a descriptor set layout");
        specs.VertexShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
        specs.FragmentShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
    }
    else
    {
        KIT_ASSERT(p_Layout, "The 3D renderer requires a descriptor set layout");
        specs.VertexShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.vert.spv";
        specs.FragmentShaderPath = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.frag.spv";
    }
    specs.PushConstantRange.size = sizeof(MeshPushConstantData<N>);

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    // WARNING!! This info points to a 'unstable' memory location. Fortunately, p_Layout still lives when the pipeline
    // is constructed (see Pipeline ctor below)
    specs.PipelineLayoutInfo.pSetLayouts = p_Layout ? &p_Layout : nullptr;
    specs.PipelineLayoutInfo.setLayoutCount = p_Layout ? 1 : 0;
    if constexpr (N == 2)
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    else
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    m_Pipeline.Create(specs);
}

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::~CircleRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Draw(const mat4 &p_ModelTransform, const vec4 &p_Color) noexcept
{
    m_DrawData.emplace_back(p_ModelTransform, p_Color);
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_DrawData.empty())
        return;

    m_Pipeline->Bind(p_Info.CommandBuffer);

    if constexpr (N == 3)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1,
                                &p_Info.GlobalDescriptorSet, 0, nullptr);

    for (const DrawData &data : m_DrawData)
    {
        MeshPushConstantData<N> pushConstantData{};
        if constexpr (N == 3)
        {
            pushConstantData.ColorAndNormalMatrix = mat4(glm::transpose(mat3(glm::inverse(data.Transform))));
            pushConstantData.ColorAndNormalMatrix[3] = data.Color;
        }
        else
            pushConstantData.Color = data.Color;

        vkCmdPushConstants(p_Info.CommandBuffer, m_Pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(MeshPushConstantData<N>), &pushConstantData);
        vkCmdDraw(p_Info.CommandBuffer, 4, 1, 0, 0);
    }
    m_DrawData.clear();
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Flush() noexcept
{
    m_DrawData.clear();
}

template class CircleRenderer<2>;
template class CircleRenderer<3>;

} // namespace ONYX