#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"

namespace Onyx::Resources
{
Result<VKit::DeviceBuffer> CreateBuffer(VKit::DeviceBufferFlags flags, VkDeviceSize instanceSize, VkDeviceSize capacity)
{
    return VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), flags)
        .SetSize(capacity, instanceSize)
        .Build();
}
} // namespace Onyx::Resources

// namespace Onyx::Detail
