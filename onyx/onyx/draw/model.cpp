#include "onyx/core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/data.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
template <Dimension D> Model<D>::Model(const std::span<const Vertex<D>> p_Vertices) noexcept
{
    m_VertexBuffer = Core::CreateVertexBuffer<D>(p_Vertices);
}

template <Dimension D>
Model<D>::Model(const std::span<const Vertex<D>> p_Vertices, const std::span<const Index> p_Indices) noexcept

{
    m_VertexBuffer = Core::CreateVertexBuffer<D>(p_Vertices);
    m_IndexBuffer = Core::CreateIndexBuffer(p_Indices);
}

template <Dimension D> Model<D>::~Model() noexcept
{
    if (m_IndexBuffer)
        m_IndexBuffer.Destroy();
}

template <Dimension D> void Model<D>::Bind(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    m_VertexBuffer.BindAsVertexBuffer(p_CommandBuffer);
    if (m_IndexBuffer)
        m_IndexBuffer.BindAsIndexBuffer(p_CommandBuffer);
}

template <Dimension D> bool Model<D>::HasIndices() const noexcept
{
    return m_IndexBuffer;
}

template <Dimension D>
void Model<D>::Draw(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount, const u32 p_FirstInstance,
                    const u32 p_FirstVertex) const noexcept
{
    TKIT_ASSERT(!m_IndexBuffer, "Model does not have indices, use Draw instead");
    vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer.GetInfo().InstanceCount), p_InstanceCount, p_FirstVertex,
              p_FirstInstance);
}

template <Dimension D>
void Model<D>::DrawIndexed(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount, const u32 p_FirstInstance,
                           const u32 p_FirstIndex, const u32 p_VertexOffset) const noexcept
{
    TKIT_ASSERT(m_IndexBuffer, "Model has indices, use DrawIndexed instead");
    vkCmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer.GetInfo().InstanceCount), p_InstanceCount,
                     p_FirstIndex, p_VertexOffset, p_FirstInstance);
}

template <Dimension D> const VertexBuffer<D> &Model<D>::GetVertexBuffer() const noexcept
{
    return m_VertexBuffer;
}
template <Dimension D> const IndexBuffer &Model<D>::GetIndexBuffer() const noexcept
{
    return m_IndexBuffer;
}

// this loads and stores the model in the user models
template <Dimension D> TKit::Scope<const Model<D>> Model<D>::Load(const std::string_view p_Path) noexcept
{
    const IndexVertexData<D> data = Onyx::Load<D>(p_Path);
    const std::span<const Vertex<D>> vertices{data.Vertices};
    const std::span<const Index> indices{data.Indices};

    const bool needsIndices = !indices.empty() && indices.size() != vertices.size();
    return needsIndices ? TKit::Scope<const Model<D>>::Create(vertices, indices)
                        : TKit::Scope<const Model<D>>::Create(vertices);
}

template class Model<D2>;
template class Model<D3>;

} // namespace Onyx