#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
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

u32 GetDrawCallCount()
{
    return s_DrawCallCount.load(std::memory_order_relaxed);
}
void ResetDrawCallCount()
{
    s_DrawCallCount.store(0, std::memory_order_relaxed);
}
#endif

template <Dimension D, PipelineMode PMode> RenderSystem<D, PMode>::RenderSystem()
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        m_DeviceSubmissionId[i] = 0;
}
template <Dimension D, PipelineMode PMode> RenderSystem<D, PMode>::~RenderSystem()
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
MeshRenderer<D, PMode>::MeshRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    this->m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const Mesh<D> &p_Mesh)
{
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];
    hostData.Data[p_Mesh].Append(p_InstanceData);
    ++hostData.Instances;
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex)
{
    this->m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        this->m_DeviceInstances += hostData.Instances;

    m_DeviceData.GrowToFit(p_FrameIndex, this->m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex)
{
    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    thread_local TKit::HashMap<Mesh<D>, TKit::StaticArray<const HostBuffer<InstanceData> *, ONYX_MAX_THREADS>>
        localData{};
    localData.clear();

    for (const auto &hostData : m_HostData)
        for (const auto &[mesh, data] : hostData.Data)
            if (!data.IsEmpty())
                localData[mesh].Append(&data);

    VKit::Buffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;
    u32 sindex = 0;

    Task mainTask{};
    for (const auto &[mesh, buffers] : localData)
        for (const auto data : buffers)
        {
            const auto copy = [&, offset] {
                TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::SendToDevice");
                storageBuffer.Write<InstanceData>(*data, {.DstOffset = offset});
            };

            if (!mainTask)
                mainTask = copy;
            else
            {
                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }
            offset += data->GetSize();
        }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}

template <DrawLevel DLevel> static VkPipelineLayout getLayout()
{
    if constexpr (DLevel == DrawLevel::Simple)
        return Core::GetGraphicsPipelineLayoutSimple();
    else
        return Core::GetGraphicsPipelineLayoutComplex();
}

template <DrawLevel DLevel> static void pushConstantData(const RenderInfo<DLevel> &p_Info)
{
    PushConstantData<DLevel> pdata{};
    pdata.ProjectionView = p_Info.Camera->ProjectionView;

    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    if constexpr (DLevel == DrawLevel::Complex)
    {
        pdata.ViewPosition = f32v4(p_Info.Camera->ViewPosition, 1.f);
        pdata.AmbientColor = p_Info.Light.AmbientColor->RGBA;
        pdata.DirectionalLightCount = p_Info.Light.DirectionalCount;
        pdata.PointLightCount = p_Info.Light.PointCount;
        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    const auto &table = Core::GetDeviceTable();
    table.CmdPushConstants(p_Info.CommandBuffer, getLayout<DLevel>(), stages, 0, sizeof(PushConstantData<DLevel>),
                           &pdata);
}

template <DrawLevel DLevel>
static void bindDescriptorSets(const RenderInfo<DLevel> &p_Info, const VkDescriptorSet p_InstanceData)
{
    if constexpr (DLevel == DrawLevel::Simple)
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, p_InstanceData,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS, getLayout<DrawLevel::Simple>());
    else
    {
        const TKit::Array<VkDescriptorSet, 2> sets = {p_InstanceData, p_Info.Light.DescriptorSet};
        const TKit::Span<const VkDescriptorSet> span(sets);
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, span, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  getLayout<DrawLevel::Complex>());
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    RenderSystem<D, PMode>::AcknowledgeSubmission(p_Info.FrameIndex);
    const u32 size = this->m_DeviceInstances * sizeof(InstanceData);

    VKit::Buffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::Buffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Render(const RenderInfo &p_Info)
{
    if (this->m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::Render");

    this->m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);
    pushConstantData<dlevel>(p_Info);

    thread_local TKit::HashMap<Mesh<D>, TKit::StaticArray<const HostBuffer<InstanceData> *, ONYX_MAX_THREADS>>
        localData{};
    localData.clear();

    u32 firstInstance = 0;
    for (const auto &hostData : m_HostData)
        for (const auto &[mesh, data] : hostData.Data)
            if (!data.IsEmpty())
                localData[mesh].Append(&data);

    for (const auto &[mesh, buffers] : localData)
    {
        u32 instanceCount = 0;
        for (const auto data : buffers)
            instanceCount += data->GetSize();

        mesh.Bind(p_Info.CommandBuffer);
        if (mesh.HasIndices())
            mesh.DrawIndexed(p_Info.CommandBuffer, instanceCount, firstInstance);
        else
            mesh.Draw(p_Info.CommandBuffer, instanceCount, firstInstance);
        INCREASE_DRAW_CALL_COUNT();
        firstInstance += instanceCount;
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Flush()
{
    RenderSystem<D, PMode>::Flush();
    for (auto &hostData : m_HostData)
    {
        for (auto &[model, data] : hostData.Data)
            data.Clear();
        hostData.Instances = 0;
    }
}

template <Dimension D, PipelineMode PMode>
PrimitiveRenderer<D, PMode>::PrimitiveRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    this->m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const u32 p_PrimitiveIndex)
{
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];
    hostData.Data[p_PrimitiveIndex].Append(p_InstanceData);
    ++hostData.Instances;
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex)
{
    this->m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        this->m_DeviceInstances += hostData.Instances;

    m_DeviceData.GrowToFit(p_FrameIndex, this->m_DeviceInstances);
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex)
{
    VKit::Buffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    constexpr u32 pcount = Primitives<D>::Count;
    const u32 hcount = m_HostData.GetSize();

    Task mainTask{};
    u32 sindex = 0;
    for (u32 i = 0; i < pcount; ++i)
        for (u32 j = 0; j < hcount; ++j)
        {
            const auto &hostData = m_HostData[j];
            const auto &data = hostData.Data[i];
            if (!data.IsEmpty())
            {
                const auto copy = [&, offset] {
                    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::SendToDevice");
                    storageBuffer.Write<InstanceData>(data, {.DstOffset = offset});
                };
                if (!mainTask)
                    mainTask = copy;
                else
                {
                    Task &task = tasks.Append(copy);
                    sindex = tm->SubmitTask(&task, sindex);
                }
                offset += data.GetSize();
            }
        }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    RenderSystem<D, PMode>::AcknowledgeSubmission(p_Info.FrameIndex);
    const u32 size = this->m_DeviceInstances * sizeof(InstanceData);

    VKit::Buffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::Buffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}
template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Render(const RenderInfo &p_Info)
{
    if (this->m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::Render");

    this->m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);

    const VKit::Buffer &vbuffer = Primitives<D>::GetVertexBuffer();
    const VKit::Buffer &ibuffer = Primitives<D>::GetIndexBuffer();

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer<Index>(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    const auto &table = Core::GetDeviceTable();

    for (u32 i = 0; i < Primitives<D>::Count; ++i)
    {
        u32 instanceCount = 0;
        for (const auto &hostData : m_HostData)
            instanceCount += hostData.Data[i].GetSize();
        if (instanceCount == 0)
            continue;

        const PrimitiveDataLayout &layout = Primitives<D>::GetDataLayout(i);

        table.CmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesCount, instanceCount, layout.IndicesStart,
                             layout.VerticesStart, firstInstance);
        INCREASE_DRAW_CALL_COUNT();
        firstInstance += instanceCount;
    }
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Flush()
{
    RenderSystem<D, PMode>::Flush();
    for (auto &hostData : m_HostData)
    {
        for (auto &data : hostData.Data)
            data.Clear();
        hostData.Instances = 0;
    }
}

template <Dimension D, PipelineMode PMode>
PolygonRenderer<D, PMode>::PolygonRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    this->m_Pipeline = PipelineGenerator<D, PMode>::CreateMeshPipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode>
void PolygonRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const TKit::Span<const f32v2> p_Vertices)
{
    TKIT_ASSERT(p_Vertices.GetSize() >= 3, "[ONYX] A polygon must have at least 3 sides");
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];

    PrimitiveDataLayout layout;
    layout.VerticesStart = hostData.Vertices.GetSize();
    layout.IndicesStart = hostData.Indices.GetSize();
    layout.IndicesCount = p_Vertices.GetSize() * 3 - 6;

    hostData.Data.Append(p_InstanceData);
    hostData.Layouts.Append(layout);

    const auto writeVertex = [&hostData](const f32v2 &p_Vertex) {
        Vertex<D> vertex{};
        if constexpr (D == D3)
        {
            vertex.Position = f32v3{p_Vertex, 0.f};
            vertex.Normal = f32v3{0.f, 0.f, 1.f};
        }
        else
            vertex.Position = p_Vertex;
        hostData.Vertices.Append(vertex);
    };

    for (u32 i = 0; i < 3; ++i)
    {
        const Index index = static_cast<Index>(i);
        hostData.Indices.Append(index);
        writeVertex(p_Vertices[i]);
    }

    for (u32 i = 3; i < p_Vertices.GetSize(); ++i)
    {
        const Index index = static_cast<Index>(i);
        hostData.Indices.Append(0);
        hostData.Indices.Append(index - 1);
        hostData.Indices.Append(index);
        writeVertex(p_Vertices[i]);
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex)
{
    this->m_DeviceInstances = 0;
    m_DeviceVertices = 0;
    m_DeviceIndices = 0;
    for (const auto &hostData : m_HostData)
    {
        this->m_DeviceInstances += hostData.Data.GetSize();
        m_DeviceVertices += hostData.Vertices.GetSize();
        m_DeviceIndices += hostData.Indices.GetSize();
    }

    m_DeviceData.GrowToFit(p_FrameIndex, this->m_DeviceInstances, m_DeviceVertices, m_DeviceIndices);
}
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex)
{
    VKit::Buffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    VKit::Buffer &vertexBuffer = m_DeviceData.StagingVertices[p_FrameIndex];
    VKit::Buffer &indexBuffer = m_DeviceData.StagingIndices[p_FrameIndex];

    u32 offset = 0;
    u32 voffset = 0;
    u32 ioffset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    u32 sindex = 0;
    const u32 hcount = m_HostData.GetSize();
    Task mainTask{};
    for (u32 i = 0; i < hcount; ++i)
    {
        const auto &hostData = m_HostData[i];
        if (!hostData.Data.IsEmpty())
        {
            const auto copy = [&, offset, voffset, ioffset] {
                TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::SendToDevice");
                storageBuffer.Write<InstanceData>(hostData.Data, {.DstOffset = offset});
                vertexBuffer.Write<Vertex<D>>(hostData.Vertices, {.DstOffset = voffset});
                indexBuffer.Write<Index>(hostData.Indices, {.DstOffset = ioffset});
            };
            if (!mainTask)
                mainTask = copy;
            else
            {
                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }

            offset += hostData.Data.GetSize();
            voffset += hostData.Vertices.GetSize();
            ioffset += hostData.Indices.GetSize();
        }
    }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    RenderSystem<D, PMode>::AcknowledgeSubmission(p_Info.FrameIndex);
    const u32 size = this->m_DeviceInstances * sizeof(InstanceData);
    const u32 vsize = m_DeviceVertices * sizeof(Vertex<D>);
    const u32 isize = m_DeviceIndices * sizeof(Index);

    VKit::Buffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::Buffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    VKit::Buffer &vbuffer = m_DeviceData.DeviceLocalVertices[p_Info.FrameIndex];
    VKit::Buffer &vstaging = m_DeviceData.StagingVertices[p_Info.FrameIndex];
    vbuffer.CopyFromBuffer(p_Info.CommandBuffer, vstaging, {.Size = vsize});

    VKit::Buffer &ibuffer = m_DeviceData.DeviceLocalIndices[p_Info.FrameIndex];
    VKit::Buffer &istaging = m_DeviceData.StagingIndices[p_Info.FrameIndex];
    ibuffer.CopyFromBuffer(p_Info.CommandBuffer, istaging, {.Size = isize});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    p_Info.AcquireVertexBarriers->Append(CreateAcquireBarrier(vbuffer, vsize, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT));
    p_Info.AcquireVertexBarriers->Append(CreateAcquireBarrier(ibuffer, isize, VK_ACCESS_INDEX_READ_BIT));
    if (p_Info.ReleaseBarriers)
    {
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(vbuffer, vsize));
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(ibuffer, isize));
    }
}
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info)
{
    if (this->m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::Render");

    this->m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);

    const VKit::Buffer &vbuffer = m_DeviceData.DeviceLocalVertices[p_Info.FrameIndex];
    const VKit::Buffer &ibuffer = m_DeviceData.DeviceLocalIndices[p_Info.FrameIndex];

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer<Index>(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    const auto &table = Core::GetDeviceTable();
    u32 firstInstance = 0;

    for (const auto &hostData : m_HostData)
        for (const PrimitiveDataLayout &layout : hostData.Layouts)
            if (layout.IndicesCount > 0)
            {
                table.CmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesCount, 1, layout.IndicesStart,
                                     layout.VerticesStart, firstInstance);
                INCREASE_DRAW_CALL_COUNT();
                ++firstInstance;
            }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Flush()
{
    RenderSystem<D, PMode>::Flush();
    for (auto &hostData : m_HostData)
    {
        hostData.Data.Clear();
        hostData.Layouts.Clear();
        hostData.Vertices.Clear();
        hostData.Indices.Clear();
    }
}

template <Dimension D, PipelineMode PMode>
CircleRenderer<D, PMode>::CircleRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    this->m_Pipeline = PipelineGenerator<D, PMode>::CreateCirclePipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode>
void CircleRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const CircleOptions &p_Options)
{
    // if (TKit::Approximately(p_Options.LowerAngle, p_Options.UpperAngle) ||
    //     TKit::Approximately(p_Options.Hollowness, 1.f))
    //     return;

    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];

    CircleInstanceData instanceData;
    instanceData.Base = p_InstanceData;

    instanceData.LowerCos = Math::Cosine(p_Options.LowerAngle);
    instanceData.LowerSin = Math::Sine(p_Options.LowerAngle);
    instanceData.UpperCos = Math::Cosine(p_Options.UpperAngle);
    instanceData.UpperSin = Math::Sine(p_Options.UpperAngle);

    instanceData.AngleOverflow = Math::Absolute(p_Options.UpperAngle - p_Options.LowerAngle) > Math::Pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    hostData.Data.Append(instanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex)
{
    this->m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        this->m_DeviceInstances += hostData.Data.GetSize();

    m_DeviceData.GrowToFit(p_FrameIndex, this->m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex)
{
    VKit::Buffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    u32 sindex = 0;
    const u32 hcount = m_HostData.GetSize();
    Task mainTask{};
    for (u32 i = 0; i < hcount; ++i)
    {
        const auto &hostData = m_HostData[i];
        if (!hostData.Data.IsEmpty())
        {
            const auto copy = [&, offset] {
                TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::SendToDevice");
                storageBuffer.Write<CircleInstanceData>(hostData.Data, {.DstOffset = offset});
            };
            if (!mainTask)
                mainTask = copy;
            else
            {
                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }
            offset += hostData.Data.GetSize();
        }
    }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    RenderSystem<D, PMode>::AcknowledgeSubmission(p_Info.FrameIndex);

    const u32 size = this->m_DeviceInstances * sizeof(CircleInstanceData);

    VKit::Buffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::Buffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info)
{
    if (this->m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::Render");

    this->m_Pipeline.Bind(p_Info.CommandBuffer);
    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);
    pushConstantData<dlevel>(p_Info);

    const auto &table = Core::GetDeviceTable();

    u32 instanceCount = 0;
    for (const auto &hostData : m_HostData)
        instanceCount += hostData.Data.GetSize();
    TKIT_ASSERT(instanceCount > 0,
                "[ONYX] Circle renderer instance count is zero, which should have trigger an early return earlier");

    table.CmdDraw(p_Info.CommandBuffer, 6, instanceCount, 0, 0);
    INCREASE_DRAW_CALL_COUNT();
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush()
{
    RenderSystem<D, PMode>::Flush();
    for (auto &hostData : m_HostData)
        hostData.Data.Clear();
}

// This is crazy
template class ONYX_API RenderSystem<D2, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API RenderSystem<D2, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API RenderSystem<D2, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API RenderSystem<D2, PipelineMode::DoStencilTestNoFill>;

template class ONYX_API RenderSystem<D3, PipelineMode::NoStencilWriteDoFill>;
template class ONYX_API RenderSystem<D3, PipelineMode::DoStencilWriteDoFill>;
template class ONYX_API RenderSystem<D3, PipelineMode::DoStencilWriteNoFill>;
template class ONYX_API RenderSystem<D3, PipelineMode::DoStencilTestNoFill>;

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
