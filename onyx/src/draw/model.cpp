#include "core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/data.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices) noexcept : m_HasIndices(false)
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices);
}

ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices,
                                     const std::span<const Index> p_Indices) noexcept
    : m_HasIndices(true)
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices);
    createIndexBuffer(p_Indices);
}

Model::~Model() noexcept
{
    if (m_HasIndices)
        m_IndexBuffer.Destroy();
    m_VertexBuffer.Destroy();
}

ONYX_DIMENSION_TEMPLATE void Model::createVertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept
{
    KIT_ASSERT(!p_Vertices.empty(), "Cannot create model with no vertices");

    Buffer::Specs specs{};
    specs.InstanceCount = p_Vertices.size();
    specs.InstanceSize = sizeof(Vertex<N>);
    specs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_VertexBuffer.Create(specs);

    Buffer::Specs stagingSpecs = specs;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    Buffer stagingBuffer(stagingSpecs);
    stagingBuffer.Map();
    stagingBuffer.Write(p_Vertices.data());
    stagingBuffer.Flush();
    stagingBuffer.Unmap();

    m_VertexBuffer->CopyFrom(stagingBuffer);
}

void Model::createIndexBuffer(const std::span<const Index> p_Indices) noexcept
{
    KIT_ASSERT(!p_Indices.empty(), "If specified, indices must not be empty");
    Buffer::Specs specs{};
    specs.InstanceCount = p_Indices.size();
    specs.InstanceSize = sizeof(Index);
    specs.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_IndexBuffer.Create(specs);

    Buffer::Specs stagingSpecs = specs;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    Buffer stagingBuffer(stagingSpecs);
    stagingBuffer.Map();
    stagingBuffer.Write(p_Indices.data());
    stagingBuffer.Flush();
    stagingBuffer.Unmap();

    m_IndexBuffer->CopyFrom(stagingBuffer);
}

void Model::Bind(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    const VkBuffer buffer = m_VertexBuffer->GetBuffer();
    const VkDeviceSize offset = 0;

    // This actually takes an array of buffers, but I am only using one
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &offset);

    if (m_HasIndices)
        vkCmdBindIndexBuffer(p_CommandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

bool Model::HasIndices() const noexcept
{
    return m_HasIndices;
}

void Model::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    if (m_HasIndices)
        vkCmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer->GetInstanceCount()), 1, 0, 0, 0);
    else
        vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer->GetInstanceCount()), 1, 0, 0);
}

const Buffer &Model::GetVertexBuffer() const noexcept
{
    return *m_VertexBuffer;
}
const Buffer &Model::GetIndexBuffer() const noexcept
{
    return *m_IndexBuffer;
}

// this loads and stores the model in the user models
ONYX_DIMENSION_TEMPLATE KIT::Scope<const Model> Model::Load(const std::string_view p_Path) noexcept
{
    const IndexVertexData<N> data = Load<N>(p_Path);
    const std::span<const Vertex<N>> vertices{data.Vertices};
    const std::span<const Index> indices{data.Indices};

    const bool needsIndices = !indices.empty() && indices.size() != vertices.size();
    return needsIndices ? KIT::Scope<const Model>::Create(vertices, indices)
                        : KIT::Scope<const Model>::Create(vertices);
}
KIT::Scope<const Model> Model::Load2D(const std::string_view p_Path) noexcept
{
    return Load<2>(p_Path);
}
KIT::Scope<const Model> Model::Load3D(const std::string_view p_Path) noexcept
{
    return Load<3>(p_Path);
}

} // namespace ONYX