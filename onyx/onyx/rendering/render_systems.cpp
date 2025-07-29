#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"

#ifdef TKIT_ENABLE_INSTRUMENTATION
#    include <atomic>
#    define INCREASE_DRAW_CALL_COUNT() s_DrawCallCount.fetch_add(1, std::memory_order_relaxed)
#else
#    define INCREASE_DRAW_CALL_COUNT()
#endif

namespace Onyx::Detail
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
static std::atomic<u32> s_DrawCallCount = 0;

u32 GetDrawCallCount() noexcept
{
    return s_DrawCallCount.load(std::memory_order_relaxed);
}
void ResetDrawCallCount() noexcept
{
    s_DrawCallCount.store(0, std::memory_order_relaxed);
}
#endif

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

template <typename T> static void initializeRenderer(T &p_DeviceData) noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo info = p_DeviceData.StorageBuffers[i].GetDescriptorInfo();
        p_DeviceData.DescriptorSets[i] = WriteStorageBufferDescriptorSet(info);
    }
}

template <Dimension D, PipelineMode PMode>
MeshRenderer<D, PMode>::MeshRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
    initializeRenderer(m_DeviceData);
}

template <Dimension D, PipelineMode PMode> MeshRenderer<D, PMode>::~MeshRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const Mesh<D> &p_Mesh) noexcept
{
    m_HostData[p_Mesh].Append(p_InstanceData);
    ++m_DeviceInstances;
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    u32 offset = 0;
    for (const auto &[mesh, data] : m_HostData)
        if (!data.IsEmpty())
        {
            storageBuffer.Write(data, offset);
            offset += data.GetSize();
        }
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
    const auto &table = Core::GetDeviceTable();
    table.CmdPushConstants(p_Info.CommandBuffer, getLayout<DLevel>(), stages, 0, sizeof(PushConstantData<DLevel>),
                           &pdata);
}

template <DrawLevel DLevel>
static void bindDescriptorSets(const RenderInfo<DLevel> &p_Info, const VkDescriptorSet p_InstanceData) noexcept
{
    if constexpr (DLevel == DrawLevel::Simple)
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, p_InstanceData,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS, getLayout<DrawLevel::Simple>());
    else
    {
        const TKit::Array<VkDescriptorSet, 2> sets = {p_InstanceData, p_Info.LightStorageBuffers};
        const TKit::Span<const VkDescriptorSet> span(sets);
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, span, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  getLayout<DrawLevel::Complex>());
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    const VkDescriptorSet instanceData = m_DeviceData.DescriptorSets[p_Info.FrameIndex];
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceData);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    for (const auto &[mesh, data] : m_HostData)
        if (!data.IsEmpty())
        {
            mesh.Bind(p_Info.CommandBuffer);
            if (mesh.HasIndices())
                mesh.DrawIndexed(p_Info.CommandBuffer, firstInstance + data.GetSize(), firstInstance);
            else
                mesh.Draw(p_Info.CommandBuffer, firstInstance + data.GetSize(), firstInstance);
            INCREASE_DRAW_CALL_COUNT();
            firstInstance += data.GetSize();
        }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Flush() noexcept
{
    for (auto &[mesh, hostData] : m_HostData)
        hostData.Clear();
    m_DeviceInstances = 0;
}

template <Dimension D, PipelineMode PMode>
PrimitiveRenderer<D, PMode>::PrimitiveRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
    initializeRenderer(m_DeviceData);
}

template <Dimension D, PipelineMode PMode> PrimitiveRenderer<D, PMode>::~PrimitiveRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const u32 p_PrimitiveIndex) noexcept
{
    m_HostData[p_PrimitiveIndex].Append(p_InstanceData);
    ++m_DeviceInstances;
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    u32 offset = 0;
    for (const auto &data : m_HostData)
        if (!data.IsEmpty())
        {
            storageBuffer.Write(data, offset);
            offset += data.GetSize();
        }
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet instanceData = m_DeviceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, instanceData);

    const auto &vbuffer = Primitives<D>::GetVertexBuffer();
    const auto &ibuffer = Primitives<D>::GetIndexBuffer();

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    const auto &table = Core::GetDeviceTable();
    for (u32 i = 0; i < m_HostData.GetSize(); ++i)
        if (!m_HostData[i].IsEmpty())
        {
            const u32 instanceCount = m_HostData[i].GetSize();
            const PrimitiveDataLayout &layout = Primitives<D>::GetDataLayout(i);

            table.CmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesCount, instanceCount, layout.IndicesStart,
                                 layout.VerticesStart, firstInstance);
            INCREASE_DRAW_CALL_COUNT();
            firstInstance += instanceCount;
        }
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Flush() noexcept
{
    for (auto &data : m_HostData)
        data.Clear();
    m_DeviceInstances = 0;
}

template <Dimension D, PipelineMode PMode>
PolygonRenderer<D, PMode>::PolygonRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
    initializeRenderer(m_DeviceData);
}

template <Dimension D, PipelineMode PMode> PolygonRenderer<D, PMode>::~PolygonRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PolygonRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData,
                                     const TKit::Span<const fvec2> p_Vertices) noexcept
{
    TKIT_ASSERT(p_Vertices.GetSize() >= 3, "[ONYX] A polygon must have at least 3 sides");

    PrimitiveDataLayout layout;
    layout.VerticesStart = m_HostData.Vertices.GetSize();
    layout.IndicesStart = m_HostData.Indices.GetSize();
    layout.IndicesCount = p_Vertices.GetSize() * 3 - 6;

    m_HostData.Data.Append(p_InstanceData);
    m_HostData.Layouts.Append(layout);

    const auto writeVertex = [this](const fvec2 &p_Vertex) {
        Vertex<D> vertex{};
        if constexpr (D == D3)
        {
            vertex.Position = fvec3{p_Vertex, 0.f};
            vertex.Normal = fvec3{0.f, 0.f, 1.f};
        }
        else
            vertex.Position = p_Vertex;
        m_HostData.Vertices.Append(vertex);
    };

    for (u32 i = 0; i < 3; ++i)
    {
        const Index index = static_cast<Index>(i);
        m_HostData.Indices.Append(index);
        writeVertex(p_Vertices[i]);
    }

    for (u32 i = 3; i < p_Vertices.GetSize(); ++i)
    {
        const Index index = static_cast<Index>(i);
        m_HostData.Indices.Append(0);
        m_HostData.Indices.Append(index - 1);
        m_HostData.Indices.Append(index);
        writeVertex(p_Vertices[i]);
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_HostData.Data.GetSize() >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_HostData.Data.GetSize());

    auto &vertexBuffer = m_DeviceData.VertexBuffers[p_FrameIndex];
    const u32 vsize = m_HostData.Vertices.GetSize();
    if (vsize >= vertexBuffer.GetInfo().InstanceCount)
    {
        vertexBuffer.Destroy();
        vertexBuffer = CreateHostVisibleVertexBuffer<D>(1 + vsize + vsize / 2);
    }

    auto &indexBuffer = m_DeviceData.IndexBuffers[p_FrameIndex];
    const u32 isize = m_HostData.Indices.GetSize();
    if (isize >= indexBuffer.GetInfo().InstanceCount)
    {
        indexBuffer.Destroy();
        indexBuffer = CreateHostVisibleIndexBuffer(1 + isize + isize / 2);
    }
}
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_HostData.Data.IsEmpty())
        return;

    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    auto &vertexBuffer = m_DeviceData.VertexBuffers[p_FrameIndex];
    auto &indexBuffer = m_DeviceData.IndexBuffers[p_FrameIndex];

    storageBuffer.Write(m_HostData.Data);
    vertexBuffer.Write(m_HostData.Vertices);
    indexBuffer.Write(m_HostData.Indices);
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostData.Data.IsEmpty())
        return;
    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet instanceData = m_DeviceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, instanceData);

    const auto &vertexBuffer = m_DeviceData.VertexBuffers[p_Info.FrameIndex];
    const auto &indexBuffer = m_DeviceData.IndexBuffers[p_Info.FrameIndex];

    vertexBuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    indexBuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    const auto &table = Core::GetDeviceTable();
    for (u32 i = 0; i < m_HostData.Layouts.GetSize(); ++i)
    {
        const PrimitiveDataLayout &layout = m_HostData.Layouts[i];
        table.CmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesCount, 1, layout.IndicesStart, layout.VerticesStart,
                             i);
        INCREASE_DRAW_CALL_COUNT();
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Flush() noexcept
{
    m_HostData.Data.Clear();
    m_HostData.Layouts.Clear();
    m_HostData.Vertices.Clear();
    m_HostData.Indices.Clear();
}

template <Dimension D, PipelineMode PMode>
CircleRenderer<D, PMode>::CircleRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateCirclePipeline(p_RenderInfo);
    initializeRenderer(m_DeviceData);
}

template <Dimension D, PipelineMode PMode> CircleRenderer<D, PMode>::~CircleRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void CircleRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const CircleOptions &p_Options) noexcept
{
    // if (TKit::Approximately(p_Options.LowerAngle, p_Options.UpperAngle) ||
    //     TKit::Approximately(p_Options.Hollowness, 1.f))
    //     return;

    CircleInstanceData instanceData;
    instanceData.BaseData = p_InstanceData;

    instanceData.ArcInfo = fvec4{glm::cos(p_Options.LowerAngle), glm::sin(p_Options.LowerAngle),
                                 glm::cos(p_Options.UpperAngle), glm::sin(p_Options.UpperAngle)};
    instanceData.AngleOverflow = glm::abs(p_Options.UpperAngle - p_Options.LowerAngle) > glm::pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    m_HostData.Append(instanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_HostData.GetSize() >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_HostData.GetSize());
}
template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    if (m_HostData.IsEmpty())
        return;

    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    storageBuffer.Write(m_HostData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostData.IsEmpty())
        return;
    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::Render");
    m_Pipeline.Bind(p_Info.CommandBuffer);

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet instanceData = m_DeviceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, instanceData);

    pushConstantData<dlevel>(p_Info);

    const auto &table = Core::GetDeviceTable();
    table.CmdDraw(p_Info.CommandBuffer, 6, m_HostData.GetSize(), 0, 0);
    INCREASE_DRAW_CALL_COUNT();
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush() noexcept
{
    m_HostData.Clear();
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
