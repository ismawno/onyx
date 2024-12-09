#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_specs.hpp"
#include "onyx/core/shaders.hpp"

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

template <Dimension D, DrawMode DMode>
PolygonDeviceInstanceData<D, DMode>::PolygonDeviceInstanceData(const usize p_Capacity) noexcept
    : DeviceInstanceData<InstanceData<D, DMode>>(p_Capacity)
{
    for (usize i = 0; i < VKIT_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VertexBuffers[i] = Core::CreateMutableVertexBuffer<D>(p_Capacity);
        IndexBuffers[i] = Core::CreateMutableIndexBuffer(p_Capacity);
    }
}
template <Dimension D, DrawMode DMode> PolygonDeviceInstanceData<D, DMode>::~PolygonDeviceInstanceData() noexcept
{
    for (usize i = 0; i < VKIT_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VertexBuffers[i].Destroy();
        IndexBuffers[i].Destroy();
    }
}

template <Dimension D, PipelineMode PMode>
static VKit::GraphicsPipeline::Specs defaultPipelineSpecs(const VKit::Shader &p_VertexShader,
                                                          const VKit::Shader &p_FragmentShader,
                                                          const VkRenderPass p_RenderPass) noexcept
{
    VKit::GraphicsPipeline::Specs specs{};
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

    specs.VertexShader = p_VertexShader;
    specs.FragmentShader = p_FragmentShader;

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    return specs;
}

template <Dimension D, PipelineMode PMode>
VKit::GraphicsPipeline::Specs Pipeline<D, PMode>::CreateMeshSpecs(const VkRenderPass p_RenderPass) noexcept
{
    VKit::GraphicsPipeline::Specs specs =
        defaultPipelineSpecs<D, PMode>(Shaders<D, GetDrawMode<PMode>()>::GetMeshVertexShader(),
                                       Shaders<D, GetDrawMode<PMode>()>::GetMeshFragmentShader(), p_RenderPass);

    specs.BindingDescriptions = Vertex<D>::GetBindingDescriptions();
    specs.AttributeDescriptions = Vertex<D>::GetAttributeDescriptions();
    return specs;
}

template <Dimension D, PipelineMode PMode>
VKit::GraphicsPipeline::Specs Pipeline<D, PMode>::CreateCircleSpecs(const VkRenderPass p_RenderPass) noexcept
{
    return defaultPipelineSpecs<D, PMode>(Shaders<D, GetDrawMode<PMode>()>::GetCircleVertexShader(),
                                          Shaders<D, GetDrawMode<PMode>()>::GetCircleFragmentShader(), p_RenderPass);
}

template struct PolygonDeviceInstanceData<D2, DrawMode::Fill>;
template struct PolygonDeviceInstanceData<D2, DrawMode::Stencil>;
template struct PolygonDeviceInstanceData<D3, DrawMode::Fill>;
template struct PolygonDeviceInstanceData<D3, DrawMode::Stencil>;

template struct Pipeline<D2, PipelineMode::NoStencilWriteDoFill>;
template struct Pipeline<D2, PipelineMode::DoStencilWriteDoFill>;
template struct Pipeline<D2, PipelineMode::DoStencilWriteNoFill>;
template struct Pipeline<D2, PipelineMode::DoStencilTestNoFill>;

template struct Pipeline<D3, PipelineMode::NoStencilWriteDoFill>;
template struct Pipeline<D3, PipelineMode::DoStencilWriteDoFill>;
template struct Pipeline<D3, PipelineMode::DoStencilWriteNoFill>;
template struct Pipeline<D3, PipelineMode::DoStencilTestNoFill>;

} // namespace Onyx