#include "core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"

namespace ONYX
{
struct PushConstantData3D
{
    vec4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

ONYX_DIMENSION_TEMPLATE struct ShaderPaths;

template <> struct ShaderPaths<2>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
};

template <> struct ShaderPaths<3>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.frag.spv";
};

ONYX_DIMENSION_TEMPLATE DeviceInstanceData<N>::DeviceInstanceData(const usize p_Capacity) noexcept
{
    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        StorageBuffers[i].Create(p_Capacity);
        StorageSizes[i] = 0;
    }
}
ONYX_DIMENSION_TEMPLATE DeviceInstanceData<N>::~DeviceInstanceData() noexcept
{
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        StorageBuffers[i].Destroy();
}

ONYX_DIMENSION_TEMPLATE static Pipeline::Specs defaultPipelineSpecs(const char *vpath, const char *fpath,
                                                                    const VkRenderPass p_RenderPass,
                                                                    const VkDescriptorSetLayout *p_Layouts,
                                                                    const u32 p_LayoutCount) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 3)
    {
        specs.PushConstantRange.size = sizeof(PushConstantData3D);
        specs.PushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        specs.PipelineLayoutInfo.pushConstantRangeCount = 1;
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    }
    else
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;

    specs.PipelineLayoutInfo.pSetLayouts = p_Layouts;
    specs.PipelineLayoutInfo.setLayoutCount = p_LayoutCount;

    specs.VertexShaderPath = vpath;
    specs.FragmentShaderPath = fpath;

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    return specs;
}

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

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::MeshRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    std::array<VkDescriptorSetLayout, N - 1> layouts;
    layouts[0] = m_DescriptorSetLayout->GetLayout();
    if constexpr (N == 3)
        layouts[1] = Core::GetLightStorageDescriptorSetLayout()->GetLayout();

    Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::MeshVertex, ShaderPaths<N>::MeshFragment,
                                                    p_RenderPass, layouts.data(), N - 1);

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);

    m_Pipeline.Create(specs);

    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::~MeshRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Draw(const u32 p_FrameIndex, const KIT::Ref<const Model<N>> &p_Model,
                                                   const InstanceData<N> &p_InstanceData) noexcept
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

static void pushConstantData(const RenderInfo<3> &p_Info, const Pipeline *p_Pipeline) noexcept
{
    PushConstantData3D pdata{};
    pdata.AmbientColor = p_Info.AmbientColor;
    pdata.DirectionalLightCount = p_Info.DirectionalLightCount;
    pdata.PointLightCount = p_Info.PointLightCount;

    vkCmdPushConstants(p_Info.CommandBuffer, p_Pipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantData3D), &pdata);
}

ONYX_DIMENSION_TEMPLATE static void bindDescriptorSets(const RenderInfo<N> &p_Info, const Pipeline *p_Pipeline,
                                                       const VkDescriptorSet p_Transforms) noexcept
{
    if constexpr (N == 2)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_Pipeline->GetLayout(), 0, 1,
                                &p_Transforms, 0, nullptr);

    else
    {
        const std::array<VkDescriptorSet, 2> sets = {p_Transforms, p_Info.LightStorageBuffers};
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_Pipeline->GetLayout(), 0,
                                static_cast<u32>(sets.size()), sets.data(), 0, nullptr);
    }
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];
    usize index = 0;
    for (const auto &[model, data] : m_HostInstanceData)
        for (const InstanceData<N> &instanceData : data)
            storageBuffer->WriteAt(index++, &instanceData);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    bindDescriptorSets<N>(p_Info, m_Pipeline.Get(), transforms);

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

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template class MeshRenderer<2>;
template class MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE PrimitiveRenderer<N>::PrimitiveRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    std::array<VkDescriptorSetLayout, N - 1> layouts;
    layouts[0] = m_DescriptorSetLayout->GetLayout();
    if constexpr (N == 3)
        layouts[1] = Core::GetLightStorageDescriptorSetLayout()->GetLayout();

    Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::MeshVertex, ShaderPaths<N>::MeshFragment,
                                                    p_RenderPass, layouts.data(), N - 1);

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);

    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE PrimitiveRenderer<N>::~PrimitiveRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Draw(const u32 p_FrameIndex, const usize p_PrimitiveIndex,
                                                        const InstanceData<N> &p_InstanceData) noexcept
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

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_DeviceInstanceData.StorageSizes[p_Info.FrameIndex] == 0)
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    usize index = 0;
    for (const auto &instanceData : m_HostInstanceData)
        for (const InstanceData<N> &data : instanceData)
            storageBuffer->WriteAt(index++, &data);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    if constexpr (N == 2)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1,
                                &transforms, 0, nullptr);

    else
    {
        const std::array<VkDescriptorSet, 2> sets = {transforms, p_Info.LightStorageBuffers};
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0,
                                static_cast<u32>(sets.size()), sets.data(), 0, nullptr);
    }

    const VertexBuffer<N> *vbuffer = Primitives<N>::GetVertexBuffer();
    const IndexBuffer *ibuffer = Primitives<N>::GetIndexBuffer();

    vbuffer->Bind(p_Info.CommandBuffer);
    ibuffer->Bind(p_Info.CommandBuffer);

    u32 firstInstance = 0;
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
    {
        if (m_HostInstanceData[i].empty())
            continue;
        const u32 size = static_cast<u32>(m_HostInstanceData[i].size());
        const PrimitiveDataLayout &layout = Primitives<N>::GetDataLayout(i);

        vkCmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesSize, size, layout.IndicesStart, layout.VerticesStart,
                         firstInstance);
        firstInstance += size;
    }
}

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Flush() noexcept
{
    for (auto &data : m_HostInstanceData)
        data.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template class PrimitiveRenderer<2>;
template class PrimitiveRenderer<3>;

ONYX_DIMENSION_TEMPLATE PolygonRenderer<N>::PolygonRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    std::array<VkDescriptorSetLayout, N - 1> layouts;
    layouts[0] = m_DescriptorSetLayout->GetLayout();
    if constexpr (N == 3)
        layouts[1] = Core::GetLightStorageDescriptorSetLayout()->GetLayout();

    Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::MeshVertex, ShaderPaths<N>::MeshFragment,
                                                    p_RenderPass, layouts.data(), N - 1);

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);

    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE PolygonRenderer<N>::~PolygonRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void PolygonRenderer<N>::Draw(const u32 p_FrameIndex, const std::span<const vec<N>> p_Vertices,
                                                      const InstanceData<N> &p_InstanceData) noexcept
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

    PolygonInstanceData instanceData;
    instanceData.Transform = p_InstanceData.Transform;
    instanceData.Material = p_InstanceData.Material;
    if constexpr (N == 3)
        instanceData.NormalMatrix = p_InstanceData.NormalMatrix;

    PrimitiveDataLayout layout;
    layout.VerticesStart = m_Vertices.size();
    layout.IndicesStart = m_Indices.size();
    layout.IndicesSize = p_Vertices.size() * 3 - 6;
    instanceData.Layout = layout;

    m_HostInstanceData.push_back(instanceData);

    const auto pushVertex = [this](const vec<N> &v) {
        Vertex<N> vertex{};
        vertex.Position = v;
        if constexpr (N == 3)
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

ONYX_DIMENSION_TEMPLATE void PolygonRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];
    auto &vertexBuffer = m_DeviceInstanceData.VertexBuffers[p_Info.FrameIndex];
    auto &indexBuffer = m_DeviceInstanceData.IndexBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
    {
        const InstanceData<N> &data = m_HostInstanceData[i]; // scary
        storageBuffer->WriteAt(i, &data);
    }
    storageBuffer->Flush();

    vertexBuffer->Write(m_Vertices);
    indexBuffer->Write(m_Indices);
    vertexBuffer->Flush();
    indexBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet transforms = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    if constexpr (N == 2)
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1,
                                &transforms, 0, nullptr);

    else
    {
        const std::array<VkDescriptorSet, 2> sets = {transforms, p_Info.LightStorageBuffers};
        vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0,
                                static_cast<u32>(sets.size()), sets.data(), 0, nullptr);
    }

    vertexBuffer->Bind(p_Info.CommandBuffer);
    indexBuffer->Bind(p_Info.CommandBuffer);

    for (const PolygonInstanceData &data : m_HostInstanceData)
        vkCmdDrawIndexed(p_Info.CommandBuffer, data.Layout.IndicesSize, 1, data.Layout.IndicesStart,
                         data.Layout.VerticesStart, 0);
}

ONYX_DIMENSION_TEMPLATE void PolygonRenderer<N>::Flush() noexcept
{
    m_HostInstanceData.clear();
}

template class PolygonRenderer<2>;
template class PolygonRenderer<3>;

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::CircleRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetTransformStorageDescriptorSetLayout();

    std::array<VkDescriptorSetLayout, N - 1> layouts;
    layouts[0] = m_DescriptorSetLayout->GetLayout();
    if constexpr (N == 3)
        layouts[1] = Core::GetLightStorageDescriptorSetLayout()->GetLayout();

    const Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::CircleVertex, ShaderPaths<N>::CircleFragment,
                                                          p_RenderPass, layouts.data(), N - 1);
    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_DeviceInstanceData.StorageBuffers[i]->GetDescriptorInfo();
        m_DeviceInstanceData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::~CircleRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Draw(const u32 p_FrameIndex,
                                                     const InstanceData<N> &p_InstanceData) noexcept
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

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_HostInstanceData.empty())
        return;

    auto &storageBuffer = m_DeviceInstanceData.StorageBuffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    for (usize i = 0; i < m_HostInstanceData.size(); ++i)
        storageBuffer->WriteAt(i, &m_HostInstanceData[i]);

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet set = m_DeviceInstanceData.DescriptorSets[p_Info.FrameIndex];
    vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1, &set,
                            0, nullptr);

    const u32 size = static_cast<u32>(m_HostInstanceData.size());
    vkCmdDraw(p_Info.CommandBuffer, 6, size, 0, 0);
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Flush() noexcept
{
    m_HostInstanceData.clear();
    for (usize i = 0; i < SwapChain::MFIF; ++i)
        m_DeviceInstanceData.StorageSizes[i] = 0;
}

template class CircleRenderer<2>;
template class CircleRenderer<3>;

} // namespace ONYX
