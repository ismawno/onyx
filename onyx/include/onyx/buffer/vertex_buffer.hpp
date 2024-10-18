#pragma once

#include "onyx/buffer/device_buffer.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/draw/vertex.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API VertexBuffer : public DeviceBuffer<Vertex<N>>
{
    KIT_NON_COPYABLE(VertexBuffer)
  public:
    VertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept;

    void Bind(VkCommandBuffer p_CommandBuffer, VkDeviceSize p_Offset = 0) const noexcept;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API MutableVertexBuffer : public Buffer
{
    KIT_NON_COPYABLE(MutableVertexBuffer)
  public:
    MutableVertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept;
    MutableVertexBuffer(usize p_Size) noexcept;

    void Bind(VkCommandBuffer p_CommandBuffer, VkDeviceSize p_Offset = 0) const noexcept;

    void Write(std::span<const Vertex<N>> p_Vertices);
};

using VertexBuffer2D = VertexBuffer<2>;
using VertexBuffer3D = VertexBuffer<3>;
} // namespace ONYX