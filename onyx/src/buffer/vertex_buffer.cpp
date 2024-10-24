#include "core/pch.hpp"
#include "onyx/buffer/vertex_buffer.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
VertexBuffer<N>::VertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept
    : DeviceBuffer<Vertex<N>>(p_Vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
{
}

template <u32 N>
    requires(IsDim<N>())
void VertexBuffer<N>::Bind(const VkCommandBuffer p_CommandBuffer, const VkDeviceSize p_Offset) const noexcept
{
    const VkBuffer buffer = this->GetBuffer();
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &p_Offset);
}

template class VertexBuffer<2>;
template class VertexBuffer<3>;

template <u32 N>
    requires(IsDim<N>())
static Buffer::Specs createBufferSpecs(const usize p_Size)
{
    Buffer::Specs specs{};
    specs.InstanceCount = p_Size;
    specs.InstanceSize = sizeof(Vertex<N>);
    specs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    specs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    specs.AllocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    return specs;
}

template <u32 N>
    requires(IsDim<N>())
MutableVertexBuffer<N>::MutableVertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept
    : Buffer(createBufferSpecs<N>(p_Vertices.size()))
{
    Map();
    Write(p_Vertices);
    Flush();
}
template <u32 N>
    requires(IsDim<N>())
MutableVertexBuffer<N>::MutableVertexBuffer(const usize p_Size) noexcept : Buffer(createBufferSpecs<N>(p_Size))
{
    Map();
}

template <u32 N>
    requires(IsDim<N>())
void MutableVertexBuffer<N>::Bind(const VkCommandBuffer p_CommandBuffer, const VkDeviceSize p_Offset) const noexcept
{
    const VkBuffer buffer = this->GetBuffer();
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &p_Offset);
}

template <u32 N>
    requires(IsDim<N>())
void MutableVertexBuffer<N>::Write(const std::span<const Vertex<N>> p_Vertices)
{
    Buffer::Write(p_Vertices.data(), p_Vertices.size() * sizeof(Vertex<N>));
}

template class MutableVertexBuffer<2>;
template class MutableVertexBuffer<3>;

} // namespace ONYX