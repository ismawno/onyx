#include "core/pch.hpp"
#include "onyx/buffer/vertex_buffer.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE VertexBuffer<N>::VertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept
    : DeviceBuffer<Vertex<N>>(p_Vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
{
}

ONYX_DIMENSION_TEMPLATE void VertexBuffer<N>::Bind(const VkCommandBuffer p_CommandBuffer,
                                                   const VkDeviceSize p_Offset) const noexcept
{
    const VkBuffer buffer = this->GetBuffer();
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &p_Offset);
}

ONYX_DIMENSION_TEMPLATE static void createBufferSpecs(const usize p_Size)
{
    Buffer::Specs specs{};
    specs.InstanceCount = p_Size;
    specs.InstanceSize = sizeof(Vertex<N>);
    specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    specs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    specs.AllocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    return specs;
}

ONYX_DIMENSION_TEMPLATE MutableVertexBuffer<N>::MutableVertexBuffer(
    const std::span<const Vertex<N>> p_Vertices) noexcept
    : Buffer(createBufferSpecs(p_Vertices.size()))
{
    Map();
    Write(p_Vertices.data());
    Flush();
}
ONYX_DIMENSION_TEMPLATE MutableVertexBuffer<N>::MutableVertexBuffer(const usize p_Size) noexcept
    : Buffer(createBufferSpecs(p_Size))
{
    Map();
}

ONYX_DIMENSION_TEMPLATE void MutableVertexBuffer<N>::Bind(const VkCommandBuffer p_CommandBuffer,
                                                          const VkDeviceSize p_Offset) const noexcept
{
    const VkBuffer buffer = this->GetBuffer();
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &p_Offset);
}

template class VertexBuffer<2>;
template class VertexBuffer<3>;

template class MutableVertexBuffer<2>;
template class MutableVertexBuffer<3>;

} // namespace ONYX