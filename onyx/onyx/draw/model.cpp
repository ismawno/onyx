#include "onyx/core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/data.hpp"
#include "onyx/core/core.hpp"
#include "tkit/utilities/hash.hpp"

namespace Onyx
{
template <Dimension D> VKit::Result<Model<D>> Model<D>::Create(const TKit::Span<const Vertex<D>> p_Vertices) noexcept
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::VertexSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Vertices;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(specs);
    if (!result)
        return VKit::Result<Model<D>>::Error(result.GetError());

    return VKit::Result<Model<D>>::Ok(result.GetValue());
}

template <Dimension D>
VKit::Result<Model<D>> Model<D>::Create(TKit::Span<const Vertex<D>> p_Vertices,
                                        TKit::Span<const Index> p_Indices) noexcept
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::VertexSpecs vspecs{};
    vspecs.Allocator = Core::GetVulkanAllocator();
    vspecs.Data = p_Vertices;
    vspecs.CommandPool = &Core::GetCommandPool();
    vspecs.Queue = Core::GetGraphicsQueue();
    auto vresult = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(vspecs);
    if (!vresult)
        return VKit::Result<Model<D>>::Error(vresult.GetError());

    typename VKit::DeviceLocalBuffer<Index>::IndexSpecs ispecs{};
    ispecs.Allocator = Core::GetVulkanAllocator();
    ispecs.Data = p_Indices;
    ispecs.CommandPool = &Core::GetCommandPool();
    ispecs.Queue = Core::GetGraphicsQueue();
    const auto iresult = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(ispecs);

    if (!iresult)
    {
        vresult.GetValue().Destroy();
        return VKit::Result<Model<D>>::Error(iresult.GetError());
    }

    return VKit::Result<Model<D>>::Ok(vresult.GetValue(), iresult.GetValue());
}

template <Dimension D> Model<D>::Model(const VertexBuffer<D> &p_VertexBuffer) noexcept : m_VertexBuffer(p_VertexBuffer)
{
}
template <Dimension D>
Model<D>::Model(const VertexBuffer<D> &p_VertexBuffer, const IndexBuffer &p_IndexBuffer) noexcept
    : m_VertexBuffer(p_VertexBuffer), m_IndexBuffer(p_IndexBuffer)
{
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
    TKIT_ASSERT(!m_IndexBuffer, "[ONYX] Model does not have indices, use Draw instead");
    vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer.GetInfo().InstanceCount), p_InstanceCount, p_FirstVertex,
              p_FirstInstance);
}

template <Dimension D>
void Model<D>::DrawIndexed(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount, const u32 p_FirstInstance,
                           const u32 p_FirstIndex, const u32 p_VertexOffset) const noexcept
{
    TKIT_ASSERT(m_IndexBuffer, "[ONYX] Model has indices, use DrawIndexed instead");
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
template <Dimension D> VKit::FormattedResult<Model<D>> Model<D>::Load(const std::string_view p_Path) noexcept
{
    const auto result = Onyx::Load<D>(p_Path);
    if (!result)
        return VKit::FormattedResult<Model>::Error(result.GetError());

    const IndexVertexData<D> &data = result.GetValue();
    const TKit::Span<const Vertex<D>> vertices{data.Vertices};
    const TKit::Span<const Index> indices{data.Indices};

    const bool needsIndices = !indices.empty() && indices.size() != vertices.size();
    return VKit::ToFormatted(needsIndices ? Model<D>::Create(vertices, indices) : Model<D>::Create(vertices));
}

template class Model<D2>;
template class Model<D3>;
} // namespace Onyx

namespace std
{
template <Onyx::Dimension D> size_t hash<Onyx::Model<D>>::operator()(const Onyx::Model<D> &p_Model) const noexcept
{
    return TKit::Hash(p_Model.GetVertexBuffer().GetBuffer(), p_Model.GetIndexBuffer().GetBuffer());
}

template struct hash<Onyx::Model<Onyx::D2>>;
template struct hash<Onyx::Model<Onyx::D3>>;

} // namespace std
