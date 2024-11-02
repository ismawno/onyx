#include "core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"

namespace ONYX
{
static VkDescriptorSet resetStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                       const DescriptorSetLayout *p_Layout,
                                                       const DescriptorPool *p_Pool,
                                                       const VkDescriptorSet p_OldSet = VK_NULL_HANDLE) noexcept
{
    if (p_OldSet)
        p_Pool->Deallocate(p_OldSet);

    DescriptorWriter writer(p_Layout, p_Pool);
    writer.WriteBuffer(0, &p_Info);
    return writer.Build();
}

template <Dimension D, PipelineMode PMode> static auto getLayouts() noexcept
{
    if constexpr (GetDrawMode<PMode>() == DrawMode::Fill)
    {
        std::array<VkDescriptorSetLayout, D - 1> layouts;
        layouts[0] = Core::GetTransformStorageDescriptorSetLayout()->GetLayout();
        if constexpr (D == D3)
            layouts[1] = Core::GetLightStorageDescriptorSetLayout()->GetLayout();
        return layouts;
    }
    else
    {
        std::array<VkDescriptorSetLayout, 1> layouts{Core::GetTransformStorageDescriptorSetLayout()->GetLayout()};
        return layouts;
    }
}

template <Dimension D, PipelineMode PMode>
MeshRenderer<D, PMode>::MeshRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    const auto layouts = getLayouts<D, PMode>();
    const Pipeline::Specs specs = CreateMeshedPipelineSpecs<D, PMode>(p_RenderPass, layouts.data(), layouts.size());
    m_Pipeline.Create(specs);

    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

template <Dimension D, PipelineMode PMode> MeshRenderer<D, PMode>::~MeshRenderer() noexcept
{
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void MeshRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                  const KIT::Ref<const Model<D>> &p_Model) noexcept
{
    m_HostInstanceData[p_Model].push_back(p_InstanceData);
    const usize size = m_DeviceInstanceData.StorageSizes[p_FrameIndex];
    auto &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];

    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                            m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }
    m_DeviceInstanceData.StorageSizes[p_FrameIndex] = size + 1;
}

static void pushConstantData(const RenderInfo<D3, DrawMode::Fill> &p_Info, const Pipeline *p_Pipeline) noexcept
{
    PushConstantData3D pdata{};
    pdata.AmbientColor = p_Info.AmbientColor;
    pdata.DirectionalLightCount = p_Info.DirectionalLightCount;
    pdata.PointLightCount = p_Info.PointLightCount;

    vkCmdPushConstants(p_Info.CommandBuffer, p_Pipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantData3D), &pdata);
}

template <Dimension D, DrawMode DMode>
static void bindDescriptorSets(const RenderInfo<D, DMode> &p_Info, const Pipeline *p_Pipeline,
                               const VkDescriptorSet p_Transforms) noexcept
{
    if constexpr (D == D2 || DMode == DrawMode::Stencil)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_Pipeline->GetLayout(), 0, 1,
                                &p_Transforms, 0, nullptr);

    else
    {
        const std::array<VkDescriptorSet, 2> sets = {p_Transforms, p_Info.LightStorageBuffers};
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_Pipeline->GetLayout(), 0,
                                static_cast<u32>(sets.size()), sets.data(), 0, nullptr);
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];
    usize index = 0;
    for (const auto &[model, data] : m_HostInstanceData)
        for (const auto &instanceData : data)
            storageBuffer->WriteAt(index++, &instanceData);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (D == D3 && GetDrawMode<PMode>() == DrawMode::Fill)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<D, GetDrawMode<PMode>()>(p_Info, m_Pipeline.Get(), transforms);

    u32 firstInstance = 0;
    for (const auto &[model, data] : m_HostInstanceData)
    {
        const u32 size = static_cast<u32>(data.size());

        model->Bind(p_Info.CommandBuffer);
        if (model->HasIndices())
            model->DrawIndexed(p_Info.CommandBuffer, firstInstance + size, firstInstance);
        else
            model->Draw(p_Info.CommandBuffer, firstInstance + size, firstInstance);
        firstInstance += size;
    }
}

template <Dimension D, PipelineMode PMode> void MeshRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template <Dimension D, PipelineMode PMode>
PrimitiveRenderer<D, PMode>::PrimitiveRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    const auto layouts = getLayouts<D, PMode>();
    const Pipeline::Specs specs = CreateMeshedPipelineSpecs<D, PMode>(p_RenderPass, layouts.data(), layouts.size());
    m_Pipeline.Create(specs);

    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

template <Dimension D, PipelineMode PMode> PrimitiveRenderer<D, PMode>::~PrimitiveRenderer() noexcept
{
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PrimitiveRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                       const usize p_PrimitiveIndex) noexcept
{
    const usize size = m_DeviceInstanceData.StorageSizes[p_FrameIndex];
    auto &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                            m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    m_HostInstanceData[p_PrimitiveIndex].push_back(p_InstanceData);
    m_DeviceInstanceData.StorageSizes[p_FrameIndex] = size + 1;
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_DeviceInstanceData.StorageSizes[p_Info.FrameIndex] == 0)
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    usize index = 0;
    for (const auto &instanceData : m_HostInstanceData)
        for (const auto &data : instanceData)
            storageBuffer->WriteAt(index++, &data);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (D == D3 && GetDrawMode<PMode>() == DrawMode::Fill)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<D, GetDrawMode<PMode>()>(p_Info, m_Pipeline.Get(), transforms);

    const VertexBuffer<D> *vbuffer = Primitives<D>::GetVertexBuffer();
    const IndexBuffer *ibuffer = Primitives<D>::GetIndexBuffer();

    vbuffer->Bind(p_Info.CommandBuffer);
    ibuffer->Bind(p_Info.CommandBuffer);

    u32 firstInstance = 0;
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
    {
        if (m_HostInstanceData[i].empty())
            continue;
        const u32 size = static_cast<u32>(m_HostInstanceData[i].size());
        const PrimitiveDataLayout &layout = Primitives<D>::GetDataLayout(i);

        vkCmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesSize, size, layout.IndicesStart, layout.VerticesStart,
                         firstInstance);
        firstInstance += size;
    }
}

template <Dimension D, PipelineMode PMode> void PrimitiveRenderer<D, PMode>::Flush() noexcept
{
    for (auto &data : m_HostInstanceData)
        data.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template <Dimension D, PipelineMode PMode>
PolygonRenderer<D, PMode>::PolygonRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    const auto layouts = getLayouts<D, PMode>();
    const Pipeline::Specs specs = CreateMeshedPipelineSpecs<D, PMode>(p_RenderPass, layouts.data(), layouts.size());
    m_Pipeline.Create(specs);

    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

template <Dimension D, PipelineMode PMode> PolygonRenderer<D, PMode>::~PolygonRenderer() noexcept
{
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void PolygonRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData,
                                     const std::span<const vec<D>> p_Vertices) noexcept
{
    KIT_ASSERT(p_Vertices.size() >= 3, "A polygon must have at least 3 sides");
    const usize storageSize = m_HostInstanceData.size();
    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    if (storageSize == storageBuffer->GetInstanceCount())
    {
        storageBuffer.Destroy();
        storageBuffer.Create(storageSize * 2);
        const VkDescriptorBufferInfo info = storageBuffer->GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                            m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    HostInstanceData instanceData;
    instanceData.Transform = p_InstanceData.Transform;

    if constexpr (GetDrawMode<PMode>() == DrawMode::Fill)
    {
        instanceData.Material = p_InstanceData.Material;
        if constexpr (D == D3)
            instanceData.NormalMatrix = p_InstanceData.NormalMatrix;
    }

    PrimitiveDataLayout layout;
    layout.VerticesStart = m_Vertices.size();
    layout.IndicesStart = m_Indices.size();
    layout.IndicesSize = p_Vertices.size() * 3 - 6;
    instanceData.Layout = layout;

    m_HostInstanceData.push_back(instanceData);

    const auto pushVertex = [this](const vec<D> &v) {
        Vertex<D> vertex{};
        vertex.Position = v;
        if constexpr (D == D3)
            vertex.Normal = vec3{0.0f, 0.0f, 1.0f};
        m_Vertices.push_back(vertex);
    };

    for (usize i = 0; i < 3; ++i)
    {
        m_Indices.push_back(static_cast<Index>(i));
        pushVertex(p_Vertices[i]);
    }

    for (usize i = 3; i < p_Vertices.size(); ++i)
    {
        const Index index = static_cast<Index>(i);
        m_Indices.push_back(0);
        m_Indices.push_back(index - 1);
        m_Indices.push_back(index);
        pushVertex(p_Vertices[i]);
    }

    const usize vertexSize = m_Vertices.size();
    const usize indexSize = m_Indices.size();
    auto &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_FrameIndex];
    auto &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_FrameIndex];

    if (vertexSize >= vertexBuffer->GetInstanceCount())
    {
        vertexBuffer.Destroy();
        vertexBuffer.Create(vertexSize * 2);
    }
    if (indexSize >= indexBuffer->GetInstanceCount())
    {
        indexBuffer.Destroy();
        indexBuffer.Create(indexSize * 2);
    }
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];
    auto &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_Info.FrameIndex];
    auto &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
    {
        const InstanceData &data = m_HostInstanceData[i]; // scary
        storageBuffer->WriteAt(i, &data);
    }
    storageBuffer->Flush();

    vertexBuffer->Write(m_Vertices);
    indexBuffer->Write(m_Indices);
    vertexBuffer->Flush();
    indexBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (D == D3 && GetDrawMode<PMode>() == DrawMode::Fill)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<D, GetDrawMode<PMode>()>(p_Info, m_Pipeline.Get(), transforms);

    vertexBuffer->Bind(p_Info.CommandBuffer);
    indexBuffer->Bind(p_Info.CommandBuffer);

    for (const auto &data : m_HostInstanceData)
        vkCmdDrawIndexed(p_Info.CommandBuffer, data.Layout.IndicesSize, 1, data.Layout.IndicesStart,
                         data.Layout.VerticesStart, 0);
}

template <Dimension D, PipelineMode PMode> void PolygonRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
}

template <Dimension D, PipelineMode PMode>
CircleRenderer<D, PMode>::CircleRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    const auto layouts = getLayouts<D, PMode>();
    const Pipeline::Specs specs = CreateCirclePipelineSpecs<D, PMode>(p_RenderPass, layouts.data(), layouts.size());
    m_Pipeline.Create(specs);

    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

template <Dimension D, PipelineMode PMode> CircleRenderer<D, PMode>::~CircleRenderer() noexcept
{
    m_Pipeline.Destroy();
}

template <Dimension D, PipelineMode PMode>
void CircleRenderer<D, PMode>::Draw(const u32 p_FrameIndex, const InstanceData &p_InstanceData) noexcept
{
    const usize size = m_HostInstanceData.size();
    auto &buffer = m_DeviceInstanceData.StorageBuffers[p_FrameIndex];
    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_DeviceInstanceData.DescriptorSets[p_FrameIndex] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                            m_DeviceInstanceData.DescriptorSets[p_FrameIndex]);
    }

    m_HostInstanceData.push_back(p_InstanceData);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Render(const RenderInfo &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
        storageBuffer->WriteAt(i, &m_HostInstanceData[i]);

    m_Pipeline->Bind(p_Info.CommandBuffer);
    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<D, GetDrawMode<PMode>()>(p_Info, m_Pipeline.Get(), transforms);

    const u32 size = static_cast<u32>(m_HostInstanceData.size());
    vkCmdDraw(p_Info.CommandBuffer, 6, size, 0, 0);
}

template <Dimension D, PipelineMode PMode> void CircleRenderer<D, PMode>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

// This is just crazy

template class MeshRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class MeshRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class MeshRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class MeshRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class MeshRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class MeshRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class MeshRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class MeshRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class PrimitiveRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class PrimitiveRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class PrimitiveRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class PrimitiveRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class PrimitiveRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class PrimitiveRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class PrimitiveRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class PrimitiveRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class PolygonRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class PolygonRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class PolygonRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class PolygonRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class PolygonRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class PolygonRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class PolygonRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class PolygonRenderer<D3, PipelineMode::DoStencilTestNoFill>;

template class CircleRenderer<D2, PipelineMode::NoStencilWriteDoFill>;
template class CircleRenderer<D2, PipelineMode::DoStencilWriteDoFill>;
template class CircleRenderer<D2, PipelineMode::DoStencilWriteNoFill>;
template class CircleRenderer<D2, PipelineMode::DoStencilTestNoFill>;

template class CircleRenderer<D3, PipelineMode::NoStencilWriteDoFill>;
template class CircleRenderer<D3, PipelineMode::DoStencilWriteDoFill>;
template class CircleRenderer<D3, PipelineMode::DoStencilWriteNoFill>;
template class CircleRenderer<D3, PipelineMode::DoStencilTestNoFill>;

} // namespace ONYX
