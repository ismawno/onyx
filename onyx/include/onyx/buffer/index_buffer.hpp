#pragma once

#include "onyx/buffer/device_buffer.hpp"
#include "onyx/core/dimension.hpp"

// AS OF RIGHT NOW THIS CANNOT BE CHANGED because VK_INDEX_TYPE_UINT32 is currently hardcoded in Bind()
#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE u32
#endif

namespace ONYX
{
using Index = ONYX_INDEX_TYPE;
class ONYX_API IndexBuffer : public DeviceBuffer<Index>
{
    KIT_NON_COPYABLE(IndexBuffer)
  public:
    IndexBuffer(const std::span<const Index> p_Vertices) noexcept;

    void Bind(VkCommandBuffer p_CommandBuffer, VkDeviceSize p_Offset = 0) const noexcept;
};
} // namespace ONYX