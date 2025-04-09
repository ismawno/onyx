#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_specs.hpp"
#include "onyx/core/shaders.hpp"

namespace Onyx
{
void ApplyCoordinateSystemExtrinsic(fmat4 &p_Transform) noexcept
{
    // Essentially, a rotation around the x axis
    for (glm::length_t i = 0; i < 4; ++i)
        for (glm::length_t j = 1; j <= 2; ++j)
            p_Transform[i][j] = -p_Transform[i][j];
}
void ApplyCoordinateSystemIntrinsic(fmat4 &p_Transform) noexcept
{
    // Essentially, a rotation around the x axis
    p_Transform[1] = -p_Transform[1];
    p_Transform[2] = -p_Transform[2];
}
} // namespace Onyx

namespace Onyx::Detail
{
template <Dimension D, DrawLevel DLevel>
PolygonDeviceInstanceData<D, DLevel>::PolygonDeviceInstanceData(const u32 p_Capacity) noexcept
    : DeviceInstanceData<InstanceData<DLevel>>(p_Capacity)
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VertexBuffers[i] = Core::CreateMutableVertexBuffer<D>(p_Capacity);
        IndexBuffers[i] = Core::CreateMutableIndexBuffer(p_Capacity);
    }
}
template <Dimension D, DrawLevel DLevel> PolygonDeviceInstanceData<D, DLevel>::~PolygonDeviceInstanceData() noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VertexBuffers[i].Destroy();
        IndexBuffers[i].Destroy();
    }
}

template <DrawLevel DLevel> static VkPipelineLayout getLayout() noexcept
{
    if constexpr (DLevel == DrawLevel::Simple)
        return Core::GetGraphicsPipelineLayoutSimple();
    else
        return Core::GetGraphicsPipelineLayoutComplex();
}

template <Dimension D, PipelineMode PMode>
static VKit::GraphicsPipeline::Builder defaultPipelineBuilder(const VkRenderPass p_RenderPass,
                                                              const VKit::Shader &p_VertexShader,
                                                              const VKit::Shader &p_FragmentShader) noexcept
{
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    VKit::GraphicsPipeline::Builder builder{Core::GetDevice(), getLayout<dlevel>(), p_RenderPass};
    auto &colorBuilder = builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .SetViewportCount(1)
                             .AddShaderStage(p_VertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(p_FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .BeginColorAttachment()
                             .EnableBlending();

    if constexpr (D == D3)
        builder.EnableDepthTest().EnableDepthWrite();
    if constexpr (GetDrawMode<PMode>() == DrawMode::Stencil && D == D2)
        colorBuilder.DisableBlending();

    const auto stencilFlags =
        VKit::GraphicsPipeline::Builder::Flag_StencilFront | VKit::GraphicsPipeline::Builder::Flag_StencilBack;
    if constexpr (PMode == PipelineMode::DoStencilWriteDoFill || PMode == PipelineMode::DoStencilWriteNoFill)
    {
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    }
    else if constexpr (PMode == PipelineMode::DoStencilTestNoFill)
    {
        builder.EnableStencilTest()
            .DisableDepthWrite()
            .SetStencilFailOperation(VK_STENCIL_OP_KEEP, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_KEEP, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_NOT_EQUAL, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0, stencilFlags)
            .SetStencilReference(1, stencilFlags);
        if constexpr (D == D3)
            builder.DisableDepthTest();
    }
    if constexpr (PMode == PipelineMode::DoStencilWriteNoFill)
        colorBuilder.SetColorWriteMask(0);
    colorBuilder.EndColorAttachment();

    return builder;
}

template <Dimension D, PipelineMode PMode>
VKit::GraphicsPipeline PipelineGenerator<D, PMode>::CreateMeshPipeline(const VkRenderPass p_RenderPass) noexcept
{
    const VKit::Shader &vertexShader = Shaders<D, GetDrawMode<PMode>()>::GetMeshVertexShader();
    const VKit::Shader &fragmentShader = Shaders<D, GetDrawMode<PMode>()>::GetMeshFragmentShader();

    VKit::GraphicsPipeline::Builder builder =
        defaultPipelineBuilder<D, PMode>(p_RenderPass, vertexShader, fragmentShader);

    builder.AddBindingDescription<Vertex<D>>(VK_VERTEX_INPUT_RATE_VERTEX);
    if constexpr (D == D2)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex<D2>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex<D3>, Position))
            .AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex<D3>, Normal));

    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D, PipelineMode PMode>
VKit::GraphicsPipeline PipelineGenerator<D, PMode>::CreateCirclePipeline(const VkRenderPass p_RenderPass) noexcept
{
    const VKit::Shader &vertexShader = Shaders<D, GetDrawMode<PMode>()>::GetCircleVertexShader();
    const VKit::Shader &fragmentShader = Shaders<D, GetDrawMode<PMode>()>::GetCircleFragmentShader();

    VKit::GraphicsPipeline::Builder builder =
        defaultPipelineBuilder<D, PMode>(p_RenderPass, vertexShader, fragmentShader);

    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template struct ONYX_API PolygonDeviceInstanceData<D2, DrawLevel::Simple>;
template struct ONYX_API PolygonDeviceInstanceData<D2, DrawLevel::Complex>;
template struct ONYX_API PolygonDeviceInstanceData<D3, DrawLevel::Simple>;
template struct ONYX_API PolygonDeviceInstanceData<D3, DrawLevel::Complex>;

template struct ONYX_API PipelineGenerator<D2, PipelineMode::NoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilWriteNoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilTestNoFill>;

template struct ONYX_API PipelineGenerator<D3, PipelineMode::NoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilWriteNoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilTestNoFill>;

} // namespace Onyx::Detail