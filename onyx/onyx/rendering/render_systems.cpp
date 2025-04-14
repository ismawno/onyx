#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/utils/math.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"

#ifdef TKIT_ENABLE_INSTRUMENTATION
#    define INCREASE_DRAW_CALL_COUNT() ++s_DrawCallCount
#else
#    define INCREASE_DRAW_CALL_COUNT()
#endif

namespace Onyx::Detail
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
static u32 s_DrawCallCount = 0;

u32 GetDrawCallCount() noexcept
{
    return s_DrawCallCount;
}
void ResetDrawCallCount() noexcept
{
    s_DrawCallCount = 0;
}
#endif

static VkDescriptorSet resetStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                       VkDescriptorSet p_OldSet = VK_NULL_HANDLE) noexcept
{
    const VKit::DescriptorSetLayout &layout = Core::GetTransformStorageDescriptorSetLayout();
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

template <typename F1, typename F2>
static void forEach(const u32 p_Start, const u32 p_End, F1 &&p_Function, F2 &&p_OnWait) noexcept
{
    TKit::ITaskManager *taskManager = Core::GetTaskManager();
    const u32 partitions = taskManager->GetThreadCount() + 1;
    TKit::Array<TKit::Ref<TKit::Task<void>>, ONYX_MAX_WORKER_THREADS> tasks;

    TKit::ForEachMainThreadLead(*taskManager, p_Start, p_End, tasks.begin(), partitions, std::forward<F1>(p_Function));
    p_OnWait();
    for (u32 i = 0; i < partitions - 1; ++i)
        tasks[i]->WaitUntilFinished();
}

template <typename T> static void initializeRenderer(T &p_DeviceInstanceData) noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo info = p_DeviceInstanceData.StorageBuffers[i].GetDescriptorInfo();
        p_DeviceInstanceData.DescriptorSets[i] = resetStorageBufferDescriptorSet(info);
    }
}

template <Dimension D, PipelineMode PMode>
MeshRenderer<D, PMode>::MeshRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderPass);
    initializeRenderer(m_DeviceInstanceData);
}

template <Dimension D, PipelineMode PMode> MeshRenderer<D, PMode>::~MeshRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                  const Model<D> &p_Model) noexcept
{
    m_HostInstanceData[p_Model].push_back(p_InstanceData);
    const u32 size = m_DeviceInstanceData.StorageSizes[p_FrameIndex];
    MutableStorageBuffer<InstanceData> &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];

    if (size == buffer.GetInfo().InstanceCount)
    {
        buffer.Destroy();
        buffer = Core::CreateMutableStorageBuffer<InstanceData>(size * 2);
        const VkDescriptorBufferInfo info = buffer.GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }
    m_DeviceInstanceData.StorageSizes[p_FrameIndex] = size + 1;
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("MeshRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    u32 index = 0;
    for (const auto &[model, data] : m_HostInstanceData)
        for (const auto &instanceData : data)
            storageBuffer.WriteAt(index++, &instanceData);
    storageBuffer.Flush();
}

template <DrawLevel DLevel> static VkPipelineLayout getLayout() noexcept
{
    if constexpr (DLevel == DrawLevel::Simple)
        return Core::GetGraphicsPipelineLayoutSimple();
    else
        return Core::GetGraphicsPipelineLayoutComplex();
}

template <DrawLevel DLevel> static void pushConstantData(const RenderInfo<DLevel> &p_Info) noexcept
{
    PushConstantData<DLevel> pdata{};
    pdata.ProjectionView = p_Info.Camera->ProjectionView;

    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    if constexpr (DLevel == DrawLevel::Complex)
    {
        pdata.ViewPosition = fvec4(p_Info.Camera->ViewPosition, 1.f);
        pdata.AmbientColor = p_Info.AmbientColor->RGBA;
        pdata.DirectionalLightCount = p_Info.DirectionalLightCount;
        pdata.PointLightCount = p_Info.PointLightCount;
        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    vkCmdPushConstants(p_Info.CommandBuffer, getLayout<DLevel>(), stages, 0, sizeof(PushConstantData<DLevel>), &pdata);
}

template <DrawLevel DLevel>
static void bindDescriptorSets(const RenderInfo<DLevel> &p_Info, const VkDescriptorSet p_Transforms) noexcept
{
    if constexpr (DLevel == DrawLevel::Simple)
        VKit::DescriptorSet::Bind(p_Info.CommandBuffer, p_Transforms, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  getLayout<DrawLevel::Simple>());
    else
    {
        const TKit::Array<VkDescriptorSet, 2> sets = {p_Transforms, p_Info.LightStorageBuffers};
        const TKit::Span<const VkDescriptorSet> span(sets);
        VKit::DescriptorSet::Bind(p_Info.CommandBuffer, span, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  getLayout<DrawLevel::Complex>());
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("MeshRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, transforms);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    for (const auto &[model, data] : m_HostInstanceData)
    {
        const u32 size = data.size();

        model.Bind(p_Info.CommandBuffer);
        if (model.HasIndices())
            model.DrawIndexed(p_Info.CommandBuffer, firstInstance + size, firstInstance);
        else
            model.Draw(p_Info.CommandBuffer, firstInstance + size, firstInstance);
        INCREASE_DRAW_CALL_COUNT();
        firstInstance += size;
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template <Dimension D, PipelineMode PMode>
PrimitiveRenderer<D, PMode>::PrimitiveRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderPass);
    initializeRenderer(m_DeviceInstanceData);
}

template <Dimension D, PipelineMode PMode> PrimitiveRenderer<D, PMode>::~PrimitiveRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                       const u32 p_PrimitiveIndex) noexcept
{
    const u32 size = m_DeviceInstanceData.StorageSizes[p_FrameIndex];
    MutableStorageBuffer<InstanceData> &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    if (size == buffer.GetInfo().InstanceCount)
    {
        buffer.Destroy();
        buffer = Core::CreateMutableStorageBuffer<InstanceData>(size * 2);
        const VkDescriptorBufferInfo info = buffer.GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    m_HostInstanceData[p_PrimitiveIndex].push_back(p_InstanceData);
    m_DeviceInstanceData.StorageSizes[p_FrameIndex] = size + 1;
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_DeviceInstanceData.StorageSizes[p_FrameIndex] == 0)
        return;

    TKIT_PROFILE_NSCOPE("PrimitiveRenderer::SendToDevice");
    MutableStorageBuffer<InstanceData> &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    u32 index = 0;
    for (const auto &instanceData : m_HostInstanceData)
        for (const auto &data : instanceData)
            storageBuffer.WriteAt(index++, &data);
    storageBuffer.Flush();
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstanceData.StorageSizes[p_Info.FrameIndex] == 0)
        return;
    TKIT_PROFILE_NSCOPE("PrimitiveRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, transforms);

    const VertexBuffer<D> &vbuffer = Primitives<D>::GetVertexBuffer();
    const IndexBuffer &ibuffer = Primitives<D>::GetIndexBuffer();

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    for (u32 i = 0; i < m_HostInstanceData.size(); ++i)
    {
        if (m_HostInstanceData[i].empty())
            continue;
        const u32 size = m_HostInstanceData[i].size();
        const PrimitiveDataLayout &layout = Primitives<D>::GetDataLayout(i);

        vkCmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesSize, size, layout.IndicesStart, layout.VerticesStart,
                         firstInstance);
        INCREASE_DRAW_CALL_COUNT();
        firstInstance += size;
    }
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Flush() noexcept
{
    for (auto &data : m_HostInstanceData)
        data.clear();
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template <Dimension D, PipelineMode PMode>
PolygonRenderer<D, PMode>::PolygonRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderPass);
    initializeRenderer(m_DeviceInstanceData);
}

template <Dimension D, PipelineMode PMode> PolygonRenderer<D, PMode>::~PolygonRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PolygonRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                     const TKit::Span<const fvec2> p_Vertices) noexcept
{
    TKIT_ASSERT(p_Vertices.size() >= 3, "[ONYX] A polygon must have at least 3 sides");
    const u32 storageSize = m_HostInstanceData.size();
    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    if (storageSize == storageBuffer.GetInfo().InstanceCount)
    {
        storageBuffer.Destroy();
        storageBuffer = Core::CreateMutableStorageBuffer<InstanceData>(storageSize * 2);
        const VkDescriptorBufferInfo info = storageBuffer.GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    PolygonInstanceData instanceData;
    instanceData.BaseData = p_InstanceData;

    PrimitiveDataLayout layout;
    layout.VerticesStart = m_Vertices.size();
    layout.IndicesStart = m_Indices.size();
    layout.IndicesSize = p_Vertices.size() * 3 - 6;
    instanceData.Layout = layout;

    m_HostInstanceData.push_back(instanceData);

    const auto pushVertex = [this](const fvec2 &v) {
        Vertex<D> vertex{};
        if constexpr (D == D3)
        {
            vertex.Position = fvec3{v, 0.f};
            vertex.Normal = fvec3{0.f, 0.f, 1.f};
        }
        else
            vertex.Position = v;

        m_Vertices.push_back(vertex);
    };

    for (u32 i = 0; i < 3; ++i)
    {
        m_Indices.push_back(static_cast<Index>(i));
        pushVertex(p_Vertices[i]);
    }

    for (u32 i = 3; i < p_Vertices.size(); ++i)
    {
        const Index index = static_cast<Index>(i);
        m_Indices.push_back(0);
        m_Indices.push_back(index - 1);
        m_Indices.push_back(index);
        pushVertex(p_Vertices[i]);
    }

    const u32 vertexSize = m_Vertices.size();
    const u32 indexSize = m_Indices.size();
    MutableVertexBuffer<D> &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_FrameIndex];
    MutableIndexBuffer &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_FrameIndex];

    if (vertexSize >= vertexBuffer.GetInfo().InstanceCount)
    {
        vertexBuffer.Destroy();
        vertexBuffer = Core::CreateMutableVertexBuffer<D>(vertexSize * 2);
    }
    if (indexSize >= indexBuffer.GetInfo().InstanceCount)
    {
        indexBuffer.Destroy();
        indexBuffer = Core::CreateMutableIndexBuffer(indexSize * 2);
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("PolygonRenderer::SendToDevice");
    MutableStorageBuffer<InstanceData> &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    MutableVertexBuffer<D> &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_FrameIndex];
    MutableIndexBuffer &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_FrameIndex];

    vertexBuffer.Write(m_Vertices);
    indexBuffer.Write(m_Indices);
    for (u32 i = 0; i < m_HostInstanceData.size(); ++i)
        storageBuffer.WriteAt(i, &m_HostInstanceData[i].BaseData);

    vertexBuffer.Flush();
    indexBuffer.Flush();
    storageBuffer.Flush();
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("PolygonRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, transforms);

    MutableVertexBuffer<D> &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_Info.FrameIndex];
    MutableIndexBuffer &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_Info.FrameIndex];

    vertexBuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    indexBuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    for (u32 i = 0; i < m_HostInstanceData.size(); ++i)
    {
        const auto &data = m_HostInstanceData[i];
        vkCmdDrawIndexed(p_Info.CommandBuffer, data.Layout.IndicesSize, 1, data.Layout.IndicesStart,
                         data.Layout.VerticesStart, 0);
        INCREASE_DRAW_CALL_COUNT();
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
}

template <Dimension D, PipelineMode PMode>
CircleRenderer<D, PMode>::CircleRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateCirclePipeline(p_RenderPass);
    initializeRenderer(m_DeviceInstanceData);
}

template <Dimension D, PipelineMode PMode> CircleRenderer<D, PMode>::~CircleRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void CircleRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                    const CircleOptions &p_Options) noexcept
{
    if (TKit::Approximately(p_Options.LowerAngle, p_Options.UpperAngle) ||
        TKit::Approximately(p_Options.Hollowness, 1.f))
        return;
    const u32 size = m_HostInstanceData.size();
    MutableStorageBuffer<CircleInstanceData> &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];

    CircleInstanceData instanceData;
    instanceData.BaseData = p_InstanceData;

    instanceData.ArcInfo = fvec4{glm::cos(p_Options.LowerAngle), glm::sin(p_Options.LowerAngle),
                                 glm::cos(p_Options.UpperAngle), glm::sin(p_Options.UpperAngle)};
    instanceData.AngleOverflow = glm::abs(p_Options.UpperAngle - p_Options.LowerAngle) > glm::pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    if (size == buffer.GetInfo().InstanceCount)
    {
        buffer.Destroy();
        buffer = Core::CreateMutableStorageBuffer<CircleInstanceData>(size * 2);
        const VkDescriptorBufferInfo info = buffer.GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    m_HostInstanceData.push_back(instanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("CircleRenderer::SendToDevice");
    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    for (u32 i = 0; i < m_HostInstanceData.size(); ++i)
        storageBuffer.WriteAt(i, &m_HostInstanceData[i]);
    storageBuffer.Flush();
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;
    TKIT_PROFILE_NSCOPE("CircleRenderer::Render");
    m_Pipeline.Bind(p_Info.CommandBuffer);

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, transforms);

    pushConstantData<dlevel>(p_Info);

    const u32 size = m_HostInstanceData.size();
    vkCmdDraw(p_Info.CommandBuffer, 6, size, 0, 0);
    INCREASE_DRAW_CALL_COUNT();
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

// This is just crazy

template class ONYX_API MeshRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API MeshRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API MeshRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API MeshRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API MeshRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API MeshRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API MeshRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API MeshRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API PrimitiveRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API PrimitiveRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API PrimitiveRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API PrimitiveRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API PrimitiveRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API PrimitiveRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API PrimitiveRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API PrimitiveRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API PolygonRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API PolygonRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API PolygonRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API PolygonRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API PolygonRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API PolygonRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API PolygonRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API PolygonRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API CircleRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API CircleRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API CircleRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API CircleRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API CircleRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API CircleRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API CircleRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API CircleRenderer<D3, PipelineMode::DoStencilTestNoFill>;

} // namespace Onyx::Detail
