#include "onyx/core/pch.hpp"
#include "onyx/data/state.hpp"
#include "onyx/core/shaders.hpp"

namespace Onyx::Detail
{
VkDescriptorSet WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info, VkDescriptorSet p_OldSet)
{
    const VKit::DescriptorSetLayout &layout = Core::GetInstanceDataStorageDescriptorSetLayout();
    const VKit::DescriptorPool &pool = Core::GetDescriptorPool();

    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &layout};
    writer.WriteBuffer(0, p_Info);

    if (!p_OldSet)
    {
        const auto result = pool.Allocate(layout);
        VKIT_ASSERT_RESULT(result);
        p_OldSet = result.GetValue();
    }
    writer.Overwrite(p_OldSet);
    return p_OldSet;
}

template <Dimension D, DrawMode DMode>
PolygonDeviceData<D, DMode>::PolygonDeviceData() : DeviceData<InstanceData<D, DMode>>()
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DeviceLocalVertices[i] = CreateDeviceBuffer<Vertex<D>>(VKit::Buffer::Flag_VertexBuffer);
        DeviceLocalIndices[i] = CreateDeviceBuffer<Index>(VKit::Buffer::Flag_IndexBuffer);
        StagingVertices[i] = CreateStagingBuffer<Vertex<D>>();
        StagingIndices[i] = CreateStagingBuffer<Index>();
    }
}
template <Dimension D, DrawMode DMode> PolygonDeviceData<D, DMode>::~PolygonDeviceData()
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DeviceLocalVertices[i].Destroy();
        DeviceLocalIndices[i].Destroy();
        StagingVertices[i].Destroy();
        StagingIndices[i].Destroy();
    }
}

template <Dimension D, DrawMode DMode>
void PolygonDeviceData<D, DMode>::GrowToFit(const u32 p_FrameIndex, const u32 p_Instances, const u32 p_Vertices,
                                            const u32 p_Indices)
{
    DeviceData<InstanceData<D, DMode>>::GrowToFit(p_FrameIndex, p_Instances);

    auto &vlbuffer = DeviceLocalVertices[p_FrameIndex];
    if (vlbuffer.GetInfo().InstanceCount < p_Vertices)
    {
        vlbuffer.Destroy();
        vlbuffer = CreateDeviceBuffer<Vertex<D>>(VKit::Buffer::Flag_VertexBuffer, 1 + p_Vertices + p_Vertices / 2);
    }

    auto &ilbuffer = DeviceLocalIndices[p_FrameIndex];
    if (ilbuffer.GetInfo().InstanceCount < p_Indices)
    {
        ilbuffer.Destroy();
        ilbuffer = CreateDeviceBuffer<Index>(VKit::Buffer::Flag_IndexBuffer, 1 + p_Indices + p_Indices / 2);
    }
    auto &vsbuffer = StagingVertices[p_FrameIndex];
    if (vsbuffer.GetInfo().InstanceCount < p_Vertices)
    {
        vsbuffer.Destroy();
        vsbuffer = CreateStagingBuffer<Vertex<D>>(1 + p_Vertices + p_Vertices / 2);
    }

    auto &isbuffer = StagingIndices[p_FrameIndex];
    if (isbuffer.GetInfo().InstanceCount < p_Indices)
    {
        isbuffer.Destroy();
        isbuffer = CreateStagingBuffer<Index>(1 + p_Indices + p_Indices / 2);
    }
}

template <DrawLevel DLevel> static VkPipelineLayout getLayout()
{
    if constexpr (DLevel == DrawLevel::Simple)
        return Core::GetGraphicsPipelineLayoutSimple();
    else
        return Core::GetGraphicsPipelineLayoutComplex();
}

template <Dimension D, PipelineMode PMode>
static VKit::GraphicsPipeline::Builder defaultPipelineBuilder(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo,
                                                              const VKit::Shader &p_VertexShader,
                                                              const VKit::Shader &p_FragmentShader)
{
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    VKit::GraphicsPipeline::Builder builder{Core::GetDevice(), getLayout<dlevel>(), p_RenderInfo};
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
VKit::GraphicsPipeline PipelineGenerator<D, PMode>::CreateMeshPipeline(
    const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const VKit::Shader &vertexShader = Shaders<D, GetDrawMode<PMode>()>::GetMeshVertexShader();
    const VKit::Shader &fragmentShader = Shaders<D, GetDrawMode<PMode>()>::GetMeshFragmentShader();

    VKit::GraphicsPipeline::Builder builder =
        defaultPipelineBuilder<D, PMode>(p_RenderInfo, vertexShader, fragmentShader);

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
VKit::GraphicsPipeline PipelineGenerator<D, PMode>::CreateCirclePipeline(
    const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const VKit::Shader &vertexShader = Shaders<D, GetDrawMode<PMode>()>::GetCircleVertexShader();
    const VKit::Shader &fragmentShader = Shaders<D, GetDrawMode<PMode>()>::GetCircleFragmentShader();

    VKit::GraphicsPipeline::Builder builder =
        defaultPipelineBuilder<D, PMode>(p_RenderInfo, vertexShader, fragmentShader);

    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template struct ONYX_API PolygonDeviceData<D2, DrawMode::Fill>;
template struct ONYX_API PolygonDeviceData<D2, DrawMode::Stencil>;
template struct ONYX_API PolygonDeviceData<D3, DrawMode::Fill>;
template struct ONYX_API PolygonDeviceData<D3, DrawMode::Stencil>;

template struct ONYX_API PipelineGenerator<D2, PipelineMode::NoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilWriteNoFill>;
template struct ONYX_API PipelineGenerator<D2, PipelineMode::DoStencilTestNoFill>;

template struct ONYX_API PipelineGenerator<D3, PipelineMode::NoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilWriteDoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilWriteNoFill>;
template struct ONYX_API PipelineGenerator<D3, PipelineMode::DoStencilTestNoFill>;

} // namespace Onyx::Detail
