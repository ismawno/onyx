#include "core/pch.hpp"
#include "onyx/model/model.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{

ONYX_DIMENSION_TEMPLATE Model<N>::Model(const std::span<const Vertex<N>> p_Vertices,
                                        const Properties p_VertexBufferProperties) noexcept
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices, p_VertexBufferProperties);
}

ONYX_DIMENSION_TEMPLATE Model<N>::Model(const std::span<const Vertex<N>> p_Vertices,
                                        const std::span<const Index> p_Indices,
                                        const Properties p_VertexBufferProperties) noexcept
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices, p_VertexBufferProperties);
    createIndexBuffer(p_Indices);
}

ONYX_DIMENSION_TEMPLATE void Model<N>::createVertexBuffer(const std::span<const Vertex<N>> p_Vertices,
                                                          const Properties p_VertexBufferProperties) noexcept
{
    KIT_ASSERT(!p_Vertices.empty(), "Cannot create model with no vertices");

    if (p_VertexBufferProperties & HOST_VISIBLE)
    {
        m_VertexBuffer = KIT::Scope<Buffer>::Create(p_Vertices.size(), sizeof(Vertex<N>),
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, p_VertexBufferProperties);

        m_VertexBuffer->Map();
        m_VertexBuffer->Write(p_Vertices.data());
        if ((p_VertexBufferProperties & HOST_COHERENT) == 0)
            m_VertexBuffer->Flush();

        // Do not unmap. Assuming buffer will be changed if these are the flags provided
        // m_VertexBuffer->Unmap();
    }
    else
    {
        m_VertexBuffer = KIT::Scope<Buffer>::Create(
            p_Vertices.size(), sizeof(Vertex<N>), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            p_VertexBufferProperties);

        Buffer stagingBuffer(p_Vertices.size(), sizeof(Vertex<N>), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        stagingBuffer.Map();
        stagingBuffer.Write(p_Vertices.data());
        stagingBuffer.Flush();
        stagingBuffer.Unmap();

        m_VertexBuffer->CopyFrom(stagingBuffer);
    }
}

ONYX_DIMENSION_TEMPLATE void Model<N>::createIndexBuffer(const std::span<const Index> p_Indices) noexcept
{
    KIT_ASSERT(!p_Indices.empty(), "If specified, indices must not be empty");
    m_IndexBuffer = KIT::Scope<Buffer>::Create(p_Indices.size(), sizeof(Index),
                                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer stagingBuffer(p_Indices.size(), sizeof(Index), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    stagingBuffer.Map();
    stagingBuffer.Write(p_Indices.data());
    stagingBuffer.Flush();
    stagingBuffer.Unmap();

    m_IndexBuffer->CopyFrom(stagingBuffer);
}

ONYX_DIMENSION_TEMPLATE void Model<N>::Bind(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    const VkBuffer buffer = m_VertexBuffer->VulkanBuffer();
    const VkDeviceSize offset = 0;

    // This actually takes an array of buffers, but I am only using one
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &offset);

    if (m_IndexBuffer)
        vkCmdBindIndexBuffer(p_CommandBuffer, m_IndexBuffer->VulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

ONYX_DIMENSION_TEMPLATE bool Model<N>::HasIndices() const noexcept
{
    return m_IndexBuffer != nullptr;
}

ONYX_DIMENSION_TEMPLATE void Model<N>::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    if (m_IndexBuffer)
        vkCmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer->Size()), 1, 0, 0, 0);
    else
        vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer->Size()), 1, 0, 0);
}

ONYX_DIMENSION_TEMPLATE const Buffer &Model<N>::VertexBuffer() const noexcept
{
    return *m_VertexBuffer;
}
ONYX_DIMENSION_TEMPLATE Buffer &Model<N>::VertexBuffer() noexcept
{
    return *m_VertexBuffer;
}

template class Model<2>;
template class Model<3>;

} // namespace ONYX