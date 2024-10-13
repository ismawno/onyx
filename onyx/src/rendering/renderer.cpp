#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"

namespace ONYX
{
struct PushConstantData3D
{
    vec4 LightDirection;
    f32 LightIntensity;
    f32 AmbientIntensity;
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

ONYX_DIMENSION_TEMPLATE static Pipeline::Specs defaultPipelineSpecs(const char *vpath, const char *fpath,
                                                                    const VkRenderPass p_RenderPass,
                                                                    const VkDescriptorSetLayout p_Layout) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 3)
    {
        specs.PushConstantRange.size = sizeof(PushConstantData3D);
        specs.PushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    }
    else
        specs.DepthStencilInfo.depthTestEnable = VK_FALSE;

    specs.VertexShaderPath = vpath;
    specs.FragmentShaderPath = fpath;

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    specs.PipelineLayoutInfo.pSetLayouts = &p_Layout;
    specs.PipelineLayoutInfo.setLayoutCount = 1;

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
    m_DescriptorSetLayout = Core::GetStorageBufferDescriptorSetLayout();

    Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::MeshVertex, ShaderPaths<N>::MeshFragment,
                                                    p_RenderPass, m_DescriptorSetLayout->GetLayout());

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);

    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_PerFrameData.Buffers[i]->GetDescriptorInfo();
        m_PerFrameData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE MeshRenderer<N>::~MeshRenderer() noexcept
{
    m_Pipeline.Destroy();
}

static mat4 transform3ToTransform4(const mat3 &p_Transform) noexcept
{
    mat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

ONYX_DIMENSION_TEMPLATE DrawData<N> createDrawData(const mat<N> &p_Transform, const vec4 &p_Color) noexcept
{
    DrawData<N> drawData;
    if constexpr (N == 3)
    {
        drawData.Transform = p_Transform;
        drawData.ColorAndNormalMatrix = mat4(glm::transpose(mat3(glm::inverse(p_Transform))));
    }
    else
    {
        drawData.Transform = transform3ToTransform4(p_Transform);
        drawData.Color = p_Color;
    }
    return drawData;
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Draw(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform,
                                                   const vec4 &p_Color, const u32 p_FrameIndex) noexcept
{
    const DrawData<N> drawData = createDrawData<N>(p_Transform, p_Color);

    m_BatchData[p_Model].push_back(drawData);
    const usize size = m_PerFrameData.Sizes[p_FrameIndex];
    auto &buffer = m_PerFrameData.Buffers[p_FrameIndex];

    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_PerFrameData.DescriptorSets[p_FrameIndex] = resetStorageBufferDescriptorSet(
            info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(), m_PerFrameData.DescriptorSets[p_FrameIndex]);
    }
    m_PerFrameData.Sizes[p_FrameIndex] = size + 1;
}

static void pushConstantData(const RenderInfo<3> &p_Info, const Pipeline *p_Pipeline) noexcept
{
    PushConstantData3D pdata{};
    pdata.LightDirection = vec4{p_Info.LightDirection, 0.f};
    pdata.LightIntensity = p_Info.LightIntensity;
    pdata.AmbientIntensity = p_Info.AmbientIntensity;

    vkCmdPushConstants(p_Info.CommandBuffer, p_Pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(PushConstantData3D), &pdata);
}

ONYX_DIMENSION_TEMPLATE void MeshRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_BatchData.empty())
        return;

    auto &storageBuffer = m_PerFrameData.Buffers[p_Info.FrameIndex];
    usize index = 0;
    for (const auto &[model, data] : m_BatchData)
        for (const DrawData<N> &drawData : data)
            storageBuffer->WriteAt(index++, &drawData);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet set = m_PerFrameData.DescriptorSets[p_Info.FrameIndex];
    vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1, &set,
                            0, nullptr);

    u32 firstInstance = 0;
    for (const auto &[model, data] : m_BatchData)
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
    m_BatchData.clear();
}

template class MeshRenderer<2>;
template class MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE PrimitiveRenderer<N>::PrimitiveRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetStorageBufferDescriptorSetLayout();

    const Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::CircleVertex, ShaderPaths<N>::CircleFragment,
                                                          p_RenderPass, m_DescriptorSetLayout->GetLayout());
    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_PerFrameData.Buffers[i]->GetDescriptorInfo();
        m_PerFrameData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE PrimitiveRenderer<N>::~PrimitiveRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Draw(const usize p_PrimitiveIndex, const mat<N> &p_Transform,
                                                        const vec4 &p_Color, const u32 p_FrameIndex) noexcept
{
    const usize size = m_PerFrameData.Sizes[p_FrameIndex];
    auto &buffer = m_PerFrameData.Buffers[p_FrameIndex];
    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_PerFrameData.DescriptorSets[p_FrameIndex] = resetStorageBufferDescriptorSet(
            info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(), m_PerFrameData.DescriptorSets[p_FrameIndex]);
    }

    const DrawData<N> drawData = createDrawData<N>(p_Transform, p_Color);
    m_BatchData[p_PrimitiveIndex].push_back(drawData);
    m_PerFrameData.Sizes[p_FrameIndex] = size + 1;
}

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_BatchData.empty())
        return;

    auto &storageBuffer = m_PerFrameData.Buffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    usize index = 0;
    for (auto &drawData : m_BatchData)
        for (const DrawData<N> &data : drawData)
            storageBuffer->WriteAt(index++, &data);
    storageBuffer->Flush();

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet set = m_PerFrameData.DescriptorSets[p_Info.FrameIndex];
    vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1, &set,
                            0, nullptr);

    const VertexBuffer<N> *vbuffer = Primitives<N>::GetVertexBuffer();
    const IndexBuffer *ibuffer = Primitives<N>::GetIndexBuffer();

    vbuffer->Bind(p_Info.CommandBuffer);
    ibuffer->Bind(p_Info.CommandBuffer);

    for (usize i = 0; i < m_BatchData.size(); ++i)
    {
        const PrimitiveDataLayout &layout = Primitives<N>::GetDataLayout(i);
        const u32 size = static_cast<u32>(m_BatchData[i].size());

        vkCmdDrawIndexed(p_Info.CommandBuffer, layout.IndicesSize, size, layout.IndicesStart, layout.VerticesStart, 0);
    }
}

ONYX_DIMENSION_TEMPLATE void PrimitiveRenderer<N>::Flush() noexcept
{
    for (auto &data : m_BatchData)
        data.clear();
}

template class PrimitiveRenderer<2>;
template class PrimitiveRenderer<3>;

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::CircleRenderer(const VkRenderPass p_RenderPass) noexcept
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetStorageBufferDescriptorSetLayout();

    const Pipeline::Specs specs = defaultPipelineSpecs<N>(ShaderPaths<N>::CircleVertex, ShaderPaths<N>::CircleFragment,
                                                          p_RenderPass, m_DescriptorSetLayout->GetLayout());
    m_Pipeline.Create(specs);

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo info = m_PerFrameData.Buffers[i]->GetDescriptorInfo();
        m_PerFrameData.DescriptorSets[i] =
            resetStorageBufferDescriptorSet(info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

ONYX_DIMENSION_TEMPLATE CircleRenderer<N>::~CircleRenderer() noexcept
{
    m_Pipeline.Destroy();
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Draw(const mat<N> &p_Transform, const vec4 &p_Color,
                                                     const u32 p_FrameIndex) noexcept
{
    const usize size = m_BatchData.size();
    auto &buffer = m_PerFrameData.Buffers[p_FrameIndex];
    if (size == buffer->GetInstanceCount())
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo info = buffer->GetDescriptorInfo();

        m_PerFrameData.DescriptorSets[p_FrameIndex] = resetStorageBufferDescriptorSet(
            info, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(), m_PerFrameData.DescriptorSets[p_FrameIndex]);
    }

    const DrawData<N> drawData = createDrawData<N>(p_Transform, p_Color);
    m_BatchData.push_back(drawData);
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Render(const RenderInfo<N> &p_Info) noexcept
{
    if (m_BatchData.empty())
        return;

    auto &storageBuffer = m_PerFrameData.Buffers[p_Info.FrameIndex];

    // Cant just memcpy this because of alignment
    for (usize i = 0; i < m_BatchData.size(); ++i)
        storageBuffer->WriteAt(i, &m_BatchData[i]);

    m_Pipeline->Bind(p_Info.CommandBuffer);
    if constexpr (N == 3)
        pushConstantData(p_Info, m_Pipeline.Get());

    const VkDescriptorSet set = m_PerFrameData.DescriptorSets[p_Info.FrameIndex];
    vkCmdBindDescriptorSets(p_Info.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetLayout(), 0, 1, &set,
                            0, nullptr);

    const u32 size = static_cast<u32>(m_BatchData.size());
    vkCmdDraw(p_Info.CommandBuffer, 6, size, 0, 0);
}

ONYX_DIMENSION_TEMPLATE void CircleRenderer<N>::Flush() noexcept
{
    m_BatchData.clear();
}

template class CircleRenderer<2>;
template class CircleRenderer<3>;

} // namespace ONYX
