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
    m_Pipeline = PipelineGenerator<D, PMode>::CreateGeometryPipeline(p_RenderInfo);
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

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::MeshRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    u32 offset = 0;
    for (const auto &hostData : m_HostData)
        for (const auto &[mesh, data] : hostData.Data)
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
    for (const auto &hostData : m_HostData)
        for (const auto &[mesh, data] : hostData.Data)
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

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::PrimitiveRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    u32 offset = 0;
    for (u32 i = 0; i < Primitives<D>::AMOUNT; ++i)
        for (const auto &hostData : m_HostData)
        {
            const auto &data = hostData.Data[i];
            if (!data.IsEmpty())
            {
                storageBuffer.Write(data, offset);
                offset += data.GetSize();
            }
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
    u32 vsize = 0;
    u32 isize = 0;
    for (const auto &hostData : m_HostData)
    {
        m_DeviceInstances += hostData.Data.GetSize();
        vsize += hostData.Vertices.GetSize();
        isize += hostData.Indices.GetSize();
    }

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);

    auto &vertexBuffer = m_DeviceData.VertexBuffers[p_FrameIndex];
    if (vsize >= vertexBuffer.GetInfo().InstanceCount)
    {
        vertexBuffer.Destroy();
        vertexBuffer = CreateHostVisibleVertexBuffer<D>(1 + vsize + vsize / 2);
    }

    auto &indexBuffer = m_DeviceData.IndexBuffers[p_FrameIndex];
    if (isize >= indexBuffer.GetInfo().InstanceCount)
    {
        indexBuffer.Destroy();
        indexBuffer = CreateHostVisibleIndexBuffer(1 + isize + isize / 2);
    }
}
template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::PolygonRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    auto &vertexBuffer = m_DeviceData.VertexBuffers[p_FrameIndex];
    auto &indexBuffer = m_DeviceData.IndexBuffers[p_FrameIndex];

    u32 offset = 0;
    u32 voffset = 0;
    u32 ioffset = 0;

    for (const auto &hostData : m_HostData)
        if (!hostData.Data.IsEmpty())
        {
            storageBuffer.Write(hostData.Data, offset);
            vertexBuffer.Write(hostData.Vertices, voffset);
            indexBuffer.Write(hostData.Indices, ioffset);

            offset += hostData.Data.GetSize();
            voffset += hostData.Vertices.GetSize();
            ioffset += hostData.Indices.GetSize();
        }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
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

    hostData.Append(instanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_HostData)
        m_DeviceInstances += hostData.GetSize();

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    if (m_DeviceInstances >= storageBuffer.GetInfo().InstanceCount)
        m_DeviceData.Grow(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::SendToDevice(const u32 p_FrameIndex) noexcept
{

    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::SendToDevice");

    auto &storageBuffer = m_DeviceData.StorageBuffers[p_FrameIndex];
    u32 offset = 0;
    for (const auto &hostData : m_HostData)
        if (!hostData.IsEmpty())
        {
            storageBuffer.Write(hostData, offset);
            offset += hostData.GetSize();
        }
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::CircleRenderer::Render");
    m_Pipeline.Bind(p_Info.CommandBuffer);

    constexpr DrawLevel dlevel = GetDrawLevel<D, PMode>();
    const VkDescriptorSet instanceData = m_DeviceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<dlevel>(p_Info, instanceData);

    pushConstantData<dlevel>(p_Info);

    const auto &table = Core::GetDeviceTable();

    u32 instanceCount = 0;
    for (const auto &hostData : m_HostData)
        instanceCount += hostData.GetSize();
    TKIT_ASSERT(instanceCount > 0,
                "[ONYX] Circle renderer instance count is zero, which should have trigger an early return earlier");

    table.CmdDraw(p_Info.CommandBuffer, 6, instanceCount, 0, 0);
    INCREASE_DRAW_CALL_COUNT();
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush() noexcept
{
    for (auto &hostData : m_HostData)
        hostData.Clear();
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
