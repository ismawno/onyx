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

template class VertexBuffer<2>;
template class VertexBuffer<3>;

} // namespace ONYX