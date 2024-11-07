#pragma once

#include "onyx/buffer/device_buffer.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/draw/vertex.hpp"

namespace ONYX
{
template <Dimension D> class ONYX_API VertexBuffer : public DeviceBuffer<Vertex<D>>
{
    KIT_NON_COPYABLE(VertexBuffer)
  public:
    VertexBuffer(const std::span<const Vertex<D>> p_Vertices) noexcept;

    void Bind(VkCommandBuffer p_CommandBuffer, VkDeviceSize p_Offset = 0) const noexcept;
};

template <Dimension D> class ONYX_API MutableVertexBuffer : public Buffer
{
    KIT_NON_COPYABLE(MutableVertexBuffer)
  public:
    MutableVertexBuffer(const std::span<const Vertex<D>> p_Vertices) noexcept;
    MutableVertexBuffer(usize p_Size) noexcept;

    void Bind(VkCommandBuffer p_CommandBuffer, VkDeviceSize p_Offset = 0) const noexcept;

    void Write(std::span<const Vertex<D>> p_Vertices);
};
} // namespace ONYX