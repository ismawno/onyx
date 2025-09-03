#include "onyx/core/pch.hpp"
#include "onyx/object/mesh.hpp"
#include "onyx/data/buffers.hpp"
#include "onyx/core/core.hpp"
#include "tkit/utils/hash.hpp"

namespace Onyx
{
template <Dimension D> VKit::Result<Mesh<D>> Mesh<D>::Create(const HostVertexBuffer<D> &p_Vertices)
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Vertices;
    specs.CommandPool = &Core::GetTransferPool();
    specs.Queue = Core::GetTransferQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    if (!result)
        return VKit::Result<Mesh<D>>::Error(result.GetError());

    return VKit::Result<Mesh<D>>::Ok(result.GetValue());
}

template <Dimension D>
VKit::Result<Mesh<D>> Mesh<D>::Create(const HostVertexBuffer<D> &p_Vertices, const HostIndexBuffer &p_Indices)
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::Specs vspecs{};
    vspecs.Allocator = Core::GetVulkanAllocator();
    vspecs.Data = p_Vertices;
    vspecs.CommandPool = &Core::GetTransferPool();
    vspecs.Queue = Core::GetTransferQueue();
    auto vresult = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), vspecs);
    if (!vresult)
        return VKit::Result<Mesh<D>>::Error(vresult.GetError());

    typename VKit::DeviceLocalBuffer<Index>::Specs ispecs{};
    ispecs.Allocator = Core::GetVulkanAllocator();
    ispecs.Data = p_Indices;
    ispecs.CommandPool = &Core::GetTransferPool();
    ispecs.Queue = Core::GetTransferQueue();
    const auto iresult = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), ispecs);

    if (!iresult)
    {
        vresult.GetValue().Destroy();
        return VKit::Result<Mesh<D>>::Error(iresult.GetError());
    }

    return VKit::Result<Mesh<D>>::Ok(vresult.GetValue(), iresult.GetValue());
}

template <Dimension D> VKit::Result<Mesh<D>> Mesh<D>::Create(const IndexVertexHostData<D> &p_Data)
{
    return Mesh<D>::Create(p_Data.Vertices, p_Data.Indices);
}

template <Dimension D>
Mesh<D>::Mesh(const DeviceLocalVertexBuffer<D> &p_VertexBuffer) : m_VertexBuffer(p_VertexBuffer)
{
}
template <Dimension D>
Mesh<D>::Mesh(const DeviceLocalVertexBuffer<D> &p_VertexBuffer, const DeviceLocalIndexBuffer &p_IndexBuffer)
    : m_VertexBuffer(p_VertexBuffer), m_IndexBuffer(p_IndexBuffer)
{
}

template <Dimension D> void Mesh<D>::Destroy()
{
    m_VertexBuffer.Destroy();
    if (m_IndexBuffer)
        m_IndexBuffer.Destroy();
}

template <Dimension D> void Mesh<D>::Bind(const VkCommandBuffer p_CommandBuffer) const
{
    m_VertexBuffer.BindAsVertexBuffer(p_CommandBuffer);
    if (m_IndexBuffer)
        m_IndexBuffer.BindAsIndexBuffer(p_CommandBuffer);
}

template <Dimension D> bool Mesh<D>::HasIndices() const
{
    return m_IndexBuffer;
}

template <Dimension D>
void Mesh<D>::Draw(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount, const u32 p_FirstInstance,
                   const u32 p_FirstVertex) const
{
    TKIT_ASSERT(!m_IndexBuffer, "[ONYX] Mesh does not have indices, use Draw instead");
    Core::GetDeviceTable().CmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer.GetInfo().InstanceCount),
                                   p_InstanceCount, p_FirstVertex, p_FirstInstance);
}

template <Dimension D>
void Mesh<D>::DrawIndexed(const VkCommandBuffer p_CommandBuffer, const u32 p_InstanceCount, const u32 p_FirstInstance,
                          const u32 p_FirstIndex, const u32 p_VertexOffset) const
{
    TKIT_ASSERT(m_IndexBuffer, "[ONYX] Mesh has indices, use DrawIndexed instead");
    Core::GetDeviceTable().CmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer.GetInfo().InstanceCount),
                                          p_InstanceCount, p_FirstIndex, p_VertexOffset, p_FirstInstance);
}

template <Dimension D> const DeviceLocalVertexBuffer<D> &Mesh<D>::GetVertexBuffer() const
{
    return m_VertexBuffer;
}
template <Dimension D> const DeviceLocalIndexBuffer &Mesh<D>::GetIndexBuffer() const
{
    return m_IndexBuffer;
}

template <Dimension D>
VKit::FormattedResult<Mesh<D>> Mesh<D>::Load(const std::string_view p_Path, const fmat<D> *p_Transform)
{
    const auto result = Onyx::Load<D>(p_Path, p_Transform);
    if (!result)
        return VKit::FormattedResult<Mesh>::Error(result.GetError());

    const IndexVertexHostData<D> &data = result.GetValue();

    // a bit of an assumption here
    const bool needsIndices = data.Indices.GetSize() > data.Vertices.GetSize();
    return VKit::ToFormatted(needsIndices ? Mesh<D>::Create(data) : Mesh<D>::Create(data.Vertices));
}

template <Dimension D> Mesh<D>::operator bool() const
{
    return m_VertexBuffer;
}
template class ONYX_API Mesh<D2>;
template class ONYX_API Mesh<D3>;
} // namespace Onyx

namespace std
{
template <Onyx::Dimension D> size_t hash<Onyx::Mesh<D>>::operator()(const Onyx::Mesh<D> &p_Mesh) const
{
    return TKit::Hash(p_Mesh.GetVertexBuffer().GetHandle(), p_Mesh.GetIndexBuffer().GetHandle());
}

template struct ONYX_API hash<Onyx::Mesh<Onyx::D2>>;
template struct ONYX_API hash<Onyx::Mesh<Onyx::D3>>;

} // namespace std
