#include "onyx/core/pch.hpp"
#include "onyx/object/mesh.hpp"
#include "onyx/data/buffers.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
template <Dimension D> VKit::Result<Mesh<D>> Mesh<D>::Create(const HostVertexBuffer<D> &p_Vertices)
{
    auto vbres = VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(),
                                       VKit::Buffer::Flag_VertexBuffer | VKit::Buffer::Flag_DeviceLocal)
                     .SetSize<Vertex<D>>(p_Vertices.GetSize())
                     .Build();

    TKIT_RETURN_ON_ERROR(vbres);

    VKit::Buffer &vbuffer = vbres.GetValue();
    QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);

    const auto ures = vbuffer.UploadFromHost<Vertex<D>>(Core::GetTransferPool(), queue->Queue, p_Vertices);
    if (!ures)
    {
        vbuffer.Destroy();
        Core::ReturnQueue(queue);
        return ures;
    }

    return VKit::Result<Mesh<D>>::Ok(vbuffer);
}

template <Dimension D>
VKit::Result<Mesh<D>> Mesh<D>::Create(const HostVertexBuffer<D> &p_Vertices, const HostIndexBuffer &p_Indices)
{
    auto vbres = VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(),
                                       VKit::Buffer::Flag_VertexBuffer | VKit::Buffer::Flag_DeviceLocal)
                     .SetSize<Vertex<D>>(p_Vertices.GetSize())
                     .Build();
    TKIT_RETURN_ON_ERROR(vbres);

    VKit::Buffer &vbuffer = vbres.GetValue();
    auto ibres = VKit::Buffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(),
                                       VKit::Buffer::Flag_IndexBuffer | VKit::Buffer::Flag_DeviceLocal)
                     .SetSize<Index>(p_Indices.GetSize())
                     .Build();
    if (!ibres)
    {
        vbuffer.Destroy();
        return ibres;
    }

    VKit::Buffer &ibuffer = ibres.GetValue();

    QueueHandle *queue = Core::BorrowQueue(VKit::Queue_Transfer);
    auto ures = vbuffer.UploadFromHost<Vertex<D>>(Core::GetTransferPool(), queue->Queue, p_Vertices);
    if (!ures)
    {
        vbuffer.Destroy();
        ibuffer.Destroy();
        Core::ReturnQueue(queue);
        return ures;
    }
    ures = ibuffer.UploadFromHost<Index>(Core::GetTransferPool(), queue->Queue, p_Indices);
    Core::ReturnQueue(queue);
    if (!ures)
    {
        vbuffer.Destroy();
        ibuffer.Destroy();
        return ures;
    }

    return VKit::Result<Mesh<D>>::Ok(vbuffer, ibuffer);
}

template <Dimension D> VKit::Result<Mesh<D>> Mesh<D>::Create(const IndexVertexHostData<D> &p_Data)
{
    return Mesh<D>::Create(p_Data.Vertices, p_Data.Indices);
}

template <Dimension D> Mesh<D>::Mesh(const VKit::Buffer &p_VertexBuffer) : m_VertexBuffer(p_VertexBuffer)
{
}
template <Dimension D>
Mesh<D>::Mesh(const VKit::Buffer &p_VertexBuffer, const VKit::Buffer &p_IndexBuffer)
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
        m_IndexBuffer.BindAsIndexBuffer<Index>(p_CommandBuffer);
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
    TKIT_RETURN_ON_ERROR(result);

    const IndexVertexHostData<D> &data = result.GetValue();

    // a bit of an assumption here
    const bool needsIndices = data.Indices.GetSize() > data.Vertices.GetSize();
    return VKit::ToFormatted(needsIndices ? Mesh<D>::Create(data) : Mesh<D>::Create(data.Vertices));
}

template class ONYX_API Mesh<D2>;
template class ONYX_API Mesh<D3>;
} // namespace Onyx
