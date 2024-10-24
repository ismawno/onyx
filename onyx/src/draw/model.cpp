#include "core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/data.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Model<N>::Model(const std::span<const Vertex<N>> p_Vertices) noexcept
    : m_VertexBuffer(p_Vertices), m_HasIndices(false)
{
    m_Device = Core::GetDevice();
}

ONYX_DIMENSION_TEMPLATE Model<N>::Model(const std::span<const Vertex<N>> p_Vertices,
                                        const std::span<const Index> p_Indices) noexcept
    : m_VertexBuffer(p_Vertices), m_HasIndices(true)
{
    m_Device = Core::GetDevice();
    m_IndexBuffer.Create(p_Indices);
}

ONYX_DIMENSION_TEMPLATE Model<N>::~Model() noexcept
{
    if (m_HasIndices)
        m_IndexBuffer.Destroy();
}

ONYX_DIMENSION_TEMPLATE void Model<N>::Bind(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    m_VertexBuffer.Bind(p_CommandBuffer);
    if (m_HasIndices)
        m_IndexBuffer->Bind(p_CommandBuffer);
}

ONYX_DIMENSION_TEMPLATE bool Model<N>::HasIndices() const noexcept
{
    return m_HasIndices;
}

ONYX_DIMENSION_TEMPLATE void Model<N>::Draw(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount,
                                            const u32 p_FirstInstance, const u32 p_FirstVertex) const noexcept
{
    KIT_ASSERT(!m_HasIndices, "Model does not have indices, use Draw instead");
    vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer.GetInstanceCount()), p_InstanceCount, p_FirstVertex,
              p_FirstInstance);
}

ONYX_DIMENSION_TEMPLATE void Model<N>::DrawIndexed(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount,
                                                   const u32 p_FirstInstance, const u32 p_FirstIndex,
                                                   const u32 p_VertexOffset) const noexcept
{
    KIT_ASSERT(m_HasIndices, "Model has indices, use DrawIndexed instead");
    vkCmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer->GetInstanceCount()), p_InstanceCount,
                     p_FirstIndex, p_VertexOffset, p_FirstInstance);
}

ONYX_DIMENSION_TEMPLATE const VertexBuffer<N> &Model<N>::GetVertexBuffer() const noexcept
{
    return m_VertexBuffer;
}
ONYX_DIMENSION_TEMPLATE const IndexBuffer &Model<N>::GetIndexBuffer() const noexcept
{
    return *m_IndexBuffer;
}

// this loads and stores the model in the user models
ONYX_DIMENSION_TEMPLATE KIT::Scope<const Model<N>> Model<N>::Load(const std::string_view p_Path) noexcept
{
    const IndexVertexData<N> data = ONYX::Load<N>(p_Path);
    const std::span<const Vertex<N>> vertices{data.Vertices};
    const std::span<const Index> indices{data.Indices};

    const bool needsIndices = !indices.empty() && indices.size() != vertices.size();
    return needsIndices ? KIT::Scope<const Model<N>>::Create(vertices, indices)
                        : KIT::Scope<const Model<N>>::Create(vertices);
}

template class Model<2>;
template class Model<3>;

} // namespace ONYX