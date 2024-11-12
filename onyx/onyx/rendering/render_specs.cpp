#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_specs.hpp"

namespace ONYX
{
void ApplyCoordinateSystem(mat4 &p_Axes, mat4 *p_InverseAxes) noexcept
{
    // Essentially, a rotation around the x axis
    for (glm::length_t i = 0; i < 4; ++i)
        for (glm::length_t j = 1; j <= 2; ++j)
            p_Axes[i][j] = -p_Axes[i][j];
    if (p_InverseAxes)
    {
        mat4 &iaxes = *p_InverseAxes;
        iaxes[1] = -iaxes[1];
        iaxes[2] = -iaxes[2];
    }
}

template <Dimension D, DrawMode DMode> struct ShaderPaths;

template <DrawMode DMode> struct ShaderPaths<D2, DMode>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
};

template <> struct ShaderPaths<D3, DrawMode::Fill>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.frag.spv";
};

template <> struct ShaderPaths<D3, DrawMode::Stencil>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh-outline3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
};

template <Dimension D, PipelineMode PMode>
static Pipeline::Specs defaultPipelineSpecs(const char *p_VPath, const char *p_FPath,
                                            const VkRenderPass p_RenderPass) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (D == D3)
    {
        specs.PushConstantRange.size = sizeof(PushConstantData3D);
        specs.PushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    }
    else if constexpr (GetDrawMode<PMode>() == DrawMode::Stencil)
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;

    specs.Layout = Core::GetRenderingPipelineLayout<D>();

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
Pipeline::Specs CreateMeshedPipelineSpecs(const VkRenderPass p_RenderPass) noexcept
{
    Pipeline::Specs specs =
        defaultPipelineSpecs<D, PMode>(ShaderPaths<D, GetDrawMode<PMode>()>::MeshVertex,
                                       ShaderPaths<D, GetDrawMode<PMode>()>::MeshFragment, p_RenderPass);

    const auto &bdesc = Vertex<D>::GetBindingDescriptions();
    const auto &attdesc = Vertex<D>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);
    return specs;
}

template <Dimension D, PipelineMode PMode>
Pipeline::Specs CreateCirclePipelineSpecs(const VkRenderPass p_RenderPass) noexcept
{
    return defaultPipelineSpecs<D, PMode>(ShaderPaths<D, GetDrawMode<PMode>()>::CircleVertex,
                                          ShaderPaths<D, GetDrawMode<PMode>()>::CircleFragment, p_RenderPass);
}

template Pipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::NoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilWriteNoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D2, PipelineMode::DoStencilTestNoFill>(VkRenderPass) noexcept;

template Pipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::NoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilWriteNoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D2, PipelineMode::DoStencilTestNoFill>(VkRenderPass) noexcept;

template Pipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::NoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilWriteNoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateMeshedPipelineSpecs<D3, PipelineMode::DoStencilTestNoFill>(VkRenderPass) noexcept;

template Pipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::NoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilWriteDoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilWriteNoFill>(VkRenderPass) noexcept;
template Pipeline::Specs CreateCirclePipelineSpecs<D3, PipelineMode::DoStencilTestNoFill>(VkRenderPass) noexcept;

template struct MeshRendererSpecs<D2, DrawMode::Fill>;
template struct MeshRendererSpecs<D2, DrawMode::Stencil>;

template struct PrimitiveRendererSpecs<D2, DrawMode::Fill>;
template struct PrimitiveRendererSpecs<D2, DrawMode::Stencil>;

template struct PolygonRendererSpecs<D2, DrawMode::Fill>;
template struct PolygonRendererSpecs<D2, DrawMode::Stencil>;

template struct CircleRendererSpecs<D2, DrawMode::Fill>;
template struct CircleRendererSpecs<D2, DrawMode::Stencil>;

template struct MeshRendererSpecs<D3, DrawMode::Fill>;
template struct MeshRendererSpecs<D3, DrawMode::Stencil>;

template struct PrimitiveRendererSpecs<D3, DrawMode::Fill>;
template struct PrimitiveRendererSpecs<D3, DrawMode::Stencil>;

template struct PolygonRendererSpecs<D3, DrawMode::Fill>;
template struct PolygonRendererSpecs<D3, DrawMode::Stencil>;

template struct CircleRendererSpecs<D3, DrawMode::Fill>;
template struct CircleRendererSpecs<D3, DrawMode::Stencil>;

} // namespace ONYX