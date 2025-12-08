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

    QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);
    specs.Queue = queue->Queue;
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    Core::ReturnQueue(queue);

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

    QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);
    vspecs.Queue = queue->Queue;

    auto vresult = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), vspecs);
    if (!vresult)
    {
        Core::ReturnQueue(queue);
        return VKit::Result<Mesh<D>>::Error(vresult.GetError());
    }

    typename VKit::DeviceLocalBuffer<Index>::Specs ispecs{};
    ispecs.Allocator = Core::GetVulkanAllocator();
    ispecs.Data = p_Indices;
    ispecs.CommandPool = &Core::GetTransferPool();
    ispecs.Queue = queue->Queue;

    const auto iresult = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), ispecs);
    Core::ReturnQueue(queue);

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

template <Dimension D> Mesh<D>::Mesh(const DeviceLocalVertexBuffer<D> &p_VertexBuffer) : m_VertexBuffer(p_VertexBuffer)
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

template <Dimension D>
VKit::FormattedResult<Mesh<D>> Mesh<D>::Load(const std::string_view p_Path, const f32m<D> *p_Transform)
{
    const auto result = Onyx::Load<D>(p_Path, p_Transform);
    if (!result)
        return VKit::FormattedResult<Mesh>::Error(result.GetError());

    const IndexVertexHostData<D> &data = result.GetValue();

    // a bit of an assumption here
    const bool needsIndices = data.Indices.GetSize() > data.Vertices.GetSize();
    return VKit::ToFormatted(needsIndices ? Mesh<D>::Create(data) : Mesh<D>::Create(data.Vertices));
}

template class ONYX_API Mesh<D2>;
template class ONYX_API Mesh<D3>;
} // namespace Onyx
