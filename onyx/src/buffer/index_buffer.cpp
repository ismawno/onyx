#include "core/pch.hpp"
#include "onyx/buffer/index_buffer.hpp"

namespace ONYX
{
IndexBuffer::IndexBuffer(const std::span<const Index> p_Indices) noexcept
    : DeviceBuffer<Index>(p_Indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
{
}

void IndexBuffer::Bind(const VkCommandBuffer p_CommandBuffer, const VkDeviceSize p_Offset) const noexcept
{
    vkCmdBindIndexBuffer(p_CommandBuffer, GetBuffer(), p_Offset, VK_INDEX_TYPE_UINT32);
}

} // namespace ONYX