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

u32 GetDrawCallCount() noexcept
{
    return s_DrawCallCount.load(std::memory_order_relaxed);
}
void ResetDrawCallCount() noexcept
{
    s_DrawCallCount.store(0, std::memory_order_relaxed);
}
#endif

template <Dimension D, PipelineMode PMode>
MeshRenderer<D, PMode>::MeshRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateGeometryPipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode> MeshRenderer<D, PMode>::~MeshRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const Mesh<D> &p_Mesh) noexcept
{
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];
    hostData.Data[p_Mesh].Append(p_InstanceData);
    ++hostData.Instances;
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        m_DeviceInstances += hostData.Instances;

    m_DeviceData.GrowToFit(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::SendToDevice");

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    thread_local TKit::HashMap<Mesh<D>, TKit::StaticArray<const HostStorageBuffer<InstanceData> *, ONYX_MAX_THREADS>>
        localData{};
    localData.clear();

    for (const auto &hostData : m_HostData)
        for (const auto &[mesh, data] : hostData.Data)
            if (!data.IsEmpty())
                localData[mesh].Append(&data);

    auto &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;
    for (const auto &[mesh, buffers] : localData)
        for (const auto data : buffers)
        {
            const Task task = tm->CreateTask([offset, &storageBuffer, data]() {
                TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::Task");
                storageBuffer.Write(*data, offset);
            });
            tasks.Append(task);
            offset += data->GetSize();
        }

    const Task task = tasks.GetBack();
    tasks.Pop();
    if (tasks.IsEmpty())
    {
        (*task)();
        tm->DestroyTask(task);
        return;
    }

    tm->SubmitTasks(TKit::Span<const Task>{tasks});

    (*task)();
    tm->DestroyTask(task);
    for (const Task task : tasks)
    {
        tm->WaitUntilFinished(task);
        tm->DestroyTask(task);
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
static void bindDescriptorSets(const RenderInfo<DLevel> &p_Info, const VkDescriptorSet p_InstanceData) noexcept
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

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info) noexcept
{
    const u32 size = m_DeviceInstances * sizeof(InstanceData);

    const auto &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    const auto &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, buffer, staging, size);

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);
    pushConstantData<dlevel>(p_Info);

    thread_local TKit::HashMap<Mesh<D>, TKit::StaticArray<const HostStorageBuffer<InstanceData> *, ONYX_MAX_THREADS>>
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

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Flush() noexcept
{
    for (auto &hostData : m_HostData)
    {
        for (auto &[model, data] : hostData.Data)
            data.Clear();
        hostData.Instances = 0;
    }
}

template <Dimension D, PipelineMode PMode> bool MeshRenderer<D, PMode>::HasInstances() const noexcept
{
    return m_DeviceInstances != 0;
}

template <Dimension D, PipelineMode PMode>
PrimitiveRenderer<D, PMode>::PrimitiveRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateGeometryPipeline(p_RenderInfo);
}

template <Dimension D, PipelineMode PMode> PrimitiveRenderer<D, PMode>::~PrimitiveRenderer() noexcept
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::Draw(const InstanceData &p_InstanceData, const u32 p_PrimitiveIndex) noexcept
{
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];
    hostData.Data[p_PrimitiveIndex].Append(p_InstanceData);
    ++hostData.Instances;
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        m_DeviceInstances += hostData.Instances;

    m_DeviceData.GrowToFit(p_FrameIndex, m_DeviceInstances);
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    for (u32 i = 0; i < Primitives<D>::AMOUNT; ++i)
        for (const auto &hostData : m_HostData)
        {
            const auto &data = hostData.Data[i];
            if (!data.IsEmpty())
            {
                const Task task = tm->CreateTask([offset, &storageBuffer, &data]() {
                    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::Task");
                    storageBuffer.Write(data, offset);
                });

                tasks.Append(task);
                offset += data.GetSize();
            }
        }

    const Task task = tasks.GetBack();
    tasks.Pop();
    if (tasks.IsEmpty())
    {
        (*task)();
        tm->DestroyTask(task);
        return;
    }

    tm->SubmitTasks(TKit::Span<const Task>{tasks});

    (*task)();
    tm->DestroyTask(task);
    for (const Task task : tasks)
    {
        tm->WaitUntilFinished(task);
        tm->DestroyTask(task);
    }
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info) noexcept
{
    const u32 size = m_DeviceInstances * sizeof(InstanceData);

    const auto &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    const auto &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, buffer, staging, size);

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}
template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);

    const auto &vbuffer = Primitives<D>::GetVertexBuffer();
    const auto &ibuffer = Primitives<D>::GetIndexBuffer();

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

    pushConstantData<dlevel>(p_Info);
    u32 firstInstance = 0;
    const auto &table = Core::GetDeviceTable();

    for (u32 i = 0; i < Primitives<D>::AMOUNT; ++i)
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

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Flush() noexcept
{
    for (auto &hostData : m_HostData)
    {
        for (auto &data : hostData.Data)
            data.Clear();
        hostData.Instances = 0;
    }
}

template <Dimension D, PipelineMode PMode> bool PrimitiveRenderer<D, PMode>::HasInstances() const noexcept
{
    return m_DeviceInstances != 0;
}

template <Dimension D, PipelineMode PMode>
PolygonRenderer<D, PMode>::PolygonRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateGeometryPipeline(p_RenderInfo);
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
    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];

    PrimitiveDataLayout layout;
    layout.VerticesStart = hostData.Vertices.GetSize();
    layout.IndicesStart = hostData.Indices.GetSize();
    layout.IndicesCount = p_Vertices.GetSize() * 3 - 6;

    hostData.Data.Append(p_InstanceData);
    hostData.Layouts.Append(layout);

    const auto writeVertex = [&hostData](const fvec2 &p_Vertex) {
        Vertex<D> vertex{};
        if constexpr (D == D3)
        {
            vertex.Position = fvec3{p_Vertex, 0.f};
            vertex.Normal = fvec3{0.f, 0.f, 1.f};
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

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_DeviceInstances = 0;
    m_DeviceVertices = 0;
    m_DeviceIndices = 0;
    for (const auto &hostData : m_HostData)
    {
        m_DeviceInstances += hostData.Data.GetSize();
        m_DeviceVertices += hostData.Vertices.GetSize();
        m_DeviceIndices += hostData.Indices.GetSize();
    }

    m_DeviceData.GrowToFit(p_FrameIndex, m_DeviceInstances, m_DeviceVertices, m_DeviceIndices);
}
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    auto &vertexBuffer = m_DeviceData.StagingVertices[p_FrameIndex];
    auto &indexBuffer = m_DeviceData.StagingIndices[p_FrameIndex];

    u32 offset = 0;
    u32 voffset = 0;
    u32 ioffset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    for (const auto &hostData : m_HostData)
        if (!hostData.Data.IsEmpty())
        {
            const Task task =
                tm->CreateTask([offset, voffset, ioffset, &hostData, &storageBuffer, &vertexBuffer, &indexBuffer]() {
                    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::Task");
                    storageBuffer.Write(hostData.Data, offset);
                    vertexBuffer.Write(hostData.Vertices, voffset);
                    indexBuffer.Write(hostData.Indices, ioffset);
                });

            offset += hostData.Data.GetSize();
            voffset += hostData.Vertices.GetSize();
            ioffset += hostData.Indices.GetSize();

            tasks.Append(task);
        }

    const Task task = tasks.GetBack();
    tasks.Pop();
    if (tasks.IsEmpty())
    {
        (*task)();
        tm->DestroyTask(task);
        return;
    }

    tm->SubmitTasks(TKit::Span<const Task>{tasks});

    (*task)();
    tm->DestroyTask(task);
    for (const Task task : tasks)
    {
        tm->WaitUntilFinished(task);
        tm->DestroyTask(task);
    }
}

template <Dimension D, PipelineMode PMode>
void PolygonRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info) noexcept
{
    const u32 size = m_DeviceInstances * sizeof(InstanceData);
    const u32 vsize = m_DeviceVertices * sizeof(Vertex<D>);
    const u32 isize = m_DeviceIndices * sizeof(Index);

    const auto &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    const auto &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, buffer, staging, size);

    const auto &vbuffer = m_DeviceData.DeviceLocalVertices[p_Info.FrameIndex];
    const auto &vstaging = m_DeviceData.StagingVertices[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, vbuffer, vstaging, vsize);

    const auto &ibuffer = m_DeviceData.DeviceLocalIndices[p_Info.FrameIndex];
    const auto &istaging = m_DeviceData.StagingIndices[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, ibuffer, istaging, isize);

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
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    bindDescriptorSets<dlevel>(p_Info, instanceDescriptor);

    const auto &vbuffer = m_DeviceData.DeviceLocalVertices[p_Info.FrameIndex];
    const auto &ibuffer = m_DeviceData.DeviceLocalIndices[p_Info.FrameIndex];

    vbuffer.BindAsVertexBuffer(p_Info.CommandBuffer);
    ibuffer.BindAsIndexBuffer(p_Info.CommandBuffer);

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

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Flush() noexcept
{
    for (auto &hostData : m_HostData)
    {
        hostData.Data.Clear();
        hostData.Layouts.Clear();
        hostData.Vertices.Clear();
        hostData.Indices.Clear();
    }
}

template <Dimension D, PipelineMode PMode> bool PolygonRenderer<D, PMode>::HasInstances() const noexcept
{
    return m_DeviceInstances != 0;
}

template <Dimension D, PipelineMode PMode>
CircleRenderer<D, PMode>::CircleRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
{
    m_Pipeline = PipelineGenerator<D, PMode>::CreateCirclePipeline(p_RenderInfo);
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

    thread_local const u32 threadIndex = Core::GetTaskManager()->GetThreadIndex();
    auto &hostData = m_HostData[threadIndex];

    CircleInstanceData instanceData;
    instanceData.BaseData = p_InstanceData;

    instanceData.ArcInfo = fvec4{glm::cos(p_Options.LowerAngle), glm::sin(p_Options.LowerAngle),
                                 glm::cos(p_Options.UpperAngle), glm::sin(p_Options.UpperAngle)};
    instanceData.AngleOverflow = glm::abs(p_Options.UpperAngle - p_Options.LowerAngle) > glm::pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    hostData.Data.Append(instanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        m_DeviceInstances += hostData.Data.GetSize();

    m_DeviceData.GrowToFit(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    for (const auto &hostData : m_HostData)
        if (!hostData.Data.IsEmpty())
        {
            const Task task = tm->CreateTask([offset, &storageBuffer, &hostData]() {
                TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::Task");
                storageBuffer.Write(hostData.Data, offset);
            });

            tasks.Append(task);
            offset += hostData.Data.GetSize();
        }

    const Task task = tasks.GetBack();
    tasks.Pop();
    if (tasks.IsEmpty())
    {
        (*task)();
        tm->DestroyTask(task);
        return;
    }

    tm->SubmitTasks(TKit::Span<const Task>{tasks});

    (*task)();
    tm->DestroyTask(task);

    for (const Task task : tasks)
    {
        tm->WaitUntilFinished(task);
        tm->DestroyTask(task);
    }
}

template <Dimension D, PipelineMode PMode>
void CircleRenderer<D, PMode>::RecordCopyCommands(const CopyInfo &p_Info) noexcept
{
    const u32 size = m_DeviceInstances * sizeof(CircleInstanceData);

    const auto &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    const auto &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    RecordCopy(p_Info.CommandBuffer, buffer, staging, size);

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
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

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush() noexcept
{
    for (auto &hostData : m_HostData)
        hostData.Data.Clear();
}

template <Dimension D, PipelineMode PMode> bool CircleRenderer<D, PMode>::HasInstances() const noexcept
{
    return m_DeviceInstances != 0;
}

// This is crazy

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
