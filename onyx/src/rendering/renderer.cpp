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

ONYX_DIMENSION_TEMPLATE struct ShaderPaths;

template <> struct ShaderPaths<2>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
};

template <> struct ShaderPaths<3>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.frag.spv";
};

ONYX_DIMENSION_TEMPLATE static Pipeline::Specs defaultMeshPipelineSpecs(const char *vpath, const char *fpath,
                                                                        const VkRenderPass p_RenderPass,
                                                                        const VkDescriptorSetLayout *p_Layout) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 2)
    {
        KIT_ASSERT(!p_Layout, "The 2D renderer does not require a descriptor set layout");
    }
    else
    {
        KIT_ASSERT(p_Layout, "The 3D renderer requires a descriptor set layout");
    }
    specs.VertexShaderPath = vpath;
    specs.FragmentShaderPath = fpath;
    specs.PushConstantRange.size = sizeof(MeshPushConstantData<N>);

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    specs.PipelineLayoutInfo.pSetLayouts = p_Layout;
    specs.PipelineLayoutInfo.setLayoutCount = p_Layout ? 1 : 0;
    if constexpr (N == 2)
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    else
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;

    return specs;
}

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::MeshRenderer(const VkRenderPass p_RenderPass,
                                                      const VkDescriptorSetLayout p_Layout) noexcept
{
    Pipeline::Specs specs = defaultMeshPipelineSpecs<N>(ShaderPaths<N>::MeshVertex, ShaderPaths<N>::MeshFragment,
                                                        p_RenderPass, p_Layout ? &p_Layout : nullptr);

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions.insert(specs.BindingDescriptions.end(), bdesc.begin(), bdesc.end());
    specs.AttributeDescriptions.insert(specs.AttributeDescriptions.end(), attdesc.begin(), attdesc.end());

    m_Pipeline.Create(specs);
}

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::~MeshRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Draw(const Model *p_Model, const mat<N> &p_ModelTransform,
                                                   const vec4 &p_Color) noexcept
{
    m_DrawData.emplace_back(p_Model, p_ModelTransform, p_Color);
}

static mat4 transform3ToTransform4(const mat3 &p_Transform) noexcept
{
    mat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

template <u32 N, typename DData>
static void pushMeshConstantData(const RenderInfo<N> &p_Info, const DData &p_Data, const Pipeline *p_Pipeline) noexcept
{
    MeshPushConstantData<N> pdata{};
    if constexpr (N == 3)
    {
        pdata.ColorAndNormalMatrix = mat4(glm::transpose(mat3(glm::inverse(p_Data.Transform))));
        pdata.ColorAndNormalMatrix[3] = p_Data.Color;

        // Consider sending the projection matrix to the shader to be combined with the model matrix there (although
        // I think this is the better way to do it)
        pdata.Transform = p_Info.Projection ? *p_Info.Projection * p_Data.Transform : p_Data.Transform;
    }
    else
    {
        pdata.Color = p_Data.Color;
        pdata.Transform = transform3ToTransform4(p_Data.Transform);
    }

    vkCmdPushConstants(p_Info.CommandBuffer, p_Pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshPushConstantData<N>), &pdata);
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Render(const RenderInfo<N> &p_Info) const noexcept
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

        pushMeshConstantData<N>(p_Info, data, m_Pipeline.Get());
        data.Model->Bind(p_Info.CommandBuffer);
        data.Model->Draw(p_Info.CommandBuffer);
    }
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
    const Pipeline::Specs specs = defaultMeshPipelineSpecs<N>(
        ShaderPaths<N>::CircleVertex, ShaderPaths<N>::CircleFragment, p_RenderPass, p_Layout ? &p_Layout : nullptr);
    m_Pipeline.Create(specs);
}

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::~CircleRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Draw(const mat<N> &p_ModelTransform, const vec4 &p_Color) noexcept
{
    m_DrawData.emplace_back(p_ModelTransform, p_Color);
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Render(const RenderInfo<N> &p_Info) const noexcept
{
    if (m_DrawData.empty())
        return;

    m_Pipeline->Bind(p_Info.CommandBuffer);

    if constexpr (N == 3)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1,
                                &p_Info.GlobalDescriptorSet, 0, nullptr);

    for (const DrawData &data : m_DrawData)
    {
        pushMeshConstantData<N>(p_Info, data, m_Pipeline.Get());
        vkCmdDraw(p_Info.CommandBuffer, 6, 1, 0, 0);
    }
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Flush() noexcept
{
    m_DrawData.clear();
}

template class CircleRenderer<2>;
template class CircleRenderer<3>;

} // namespace ONYX
