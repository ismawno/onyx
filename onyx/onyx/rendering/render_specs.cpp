#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_specs.hpp"

namespace Onyx
{
void ApplyCoordinateSystemExtrinsic(mat4 &p_Transform) noexcept
{
    // Essentially, a rotation around the x axis
    for (glm::length_t i = 0; i < 4; ++i)
        for (glm::length_t j = 1; j <= 2; ++j)
            p_Transform[i][j] = -p_Transform[i][j];
}
void ApplyCoordinateSystemIntrinsic(mat4 &p_Transform) noexcept
{
    // Essentially, a rotation around the x axis
    p_Transform[1] = -p_Transform[1];
    p_Transform[2] = -p_Transform[2];
}

template <Dimension D, DrawMode DMode> struct ShaderPaths;

template <DrawMode DMode> struct ShaderPaths<D2, DMode>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/mesh2D.vert";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/mesh2D.frag";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/circle2D.vert";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/circle2D.frag";
};

template <> struct ShaderPaths<D3, DrawMode::Fill>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/mesh3D.vert";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/mesh3D.frag";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/circle3D.vert";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/circle3D.frag";
};

template <> struct ShaderPaths<D3, DrawMode::Stencil>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/mesh-outline3D.vert";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/mesh2D.frag";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/circle2D.vert";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/circle2D.frag";
};

template <Dimension D, PipelineMode PMode>
static GraphicsPipeline::Specs defaultPipelineSpecs(const char *p_VPath, const char *p_FPath,
                                                    const VkRenderPass p_RenderPass) noexcept
{
    GraphicsPipeline::Specs specs{};
    if constexpr (D == D3)
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    else if constexpr (GetDrawMode<PMode>() == DrawMode::Stencil)
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;

    specs.Layout = Core::GetGraphicsPipelineLayout<D>();

    if constexpr (PMode == PipelineMode::DoStencilWriteDoFill || PMode == PipelineMode::DoStencilWriteNoFill)
    {
        specs.DepthStencilInfo.stencilTestEnable = VK_TRUE;
        specs.DepthStencilInfo.front.failOp = VK_STENCIL_OP_REPLACE;
        specs.DepthStencilInfo.front.passOp = VK_STENCIL_OP_REPLACE;
        specs.DepthStencilInfo.front.depthFailOp = VK_STENCIL_OP_REPLACE;
        specs.DepthStencilInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
        specs.DepthStencilInfo.front.compareMask = 0xFF;
        specs.DepthStencilInfo.front.writeMask = 0xFF;
        specs.DepthStencilInfo.front.reference = 1;
        specs.DepthStencilInfo.back = specs.DepthStencilInfo.front;
    }
    else if constexpr (PMode == PipelineMode::DoStencilTestNoFill)
    {
        specs.DepthStencilInfo.stencilTestEnable = VK_TRUE;
        specs.DepthStencilInfo.depthWriteEnable = VK_FALSE;
        specs.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        specs.DepthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
        specs.DepthStencilInfo.front.passOp = VK_STENCIL_OP_REPLACE;
        specs.DepthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
        specs.DepthStencilInfo.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        specs.DepthStencilInfo.front.reference = 1;
        specs.DepthStencilInfo.front.compareMask = 0xFF;
        specs.DepthStencilInfo.front.writeMask = 0;
        specs.DepthStencilInfo.back = specs.DepthStencilInfo.front;
        if constexpr (D == D3)
            specs.DepthStencilInfo.depthTestEnable = VK_FALSE;
    }
    if constexpr (PMode == PipelineMode::DoStencilWriteNoFill)
        specs.ColorBlendAttachment.colorWriteMask = 0;

    specs.VertexShaderPath = p_VPath;
    specs.FragmentShaderPath = p_FPath;

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    return specs;
}

template <Dimension D, PipelineMode PMode>
GraphicsPipeline::Specs CreateMeshedPipelineSpecs(const VkRenderPass p_RenderPass) noexcept
{
    GraphicsPipeline::Specs specs =
        defaultPipelineSpecs<D, PMode>(ShaderPaths<D, GetDrawMode<PMode>()>::MeshVertex,
                                       ShaderPaths<D, GetDrawMode<PMode>()>::MeshFragment, p_RenderPass);

    const auto &bdesc = Vertex<D>::GetBindingDescriptions();
    const auto &attdesc = Vertex<D>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);
    return specs;
}

template <Dimension D, PipelineMode PMode>
GraphicsPipeline::Specs CreateCirclePipelineSpecs(const VkRenderPass p_RenderPass) noexcept
{
    return defaultPipelineSpecs<D, PMode>(ShaderPaths<D, GetDrawMode<PMode>()>::CircleVertex,
                                          ShaderPaths<D, GetDrawMode<PMode>()>::CircleFragment, p_RenderPass);
}

template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::NoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilWriteNoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilTestNoFill>(
    VkRenderPass) noexcept;

template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::NoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilWriteNoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilTestNoFill>(
    VkRenderPass) noexcept;

template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::NoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilWriteNoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilTestNoFill>(
    VkRenderPass) noexcept;

template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::NoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilWriteDoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilWriteNoFill>(
    VkRenderPass) noexcept;
template GraphicsPipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilTestNoFill>(
    VkRenderPass) noexcept;

} // namespace Onyx