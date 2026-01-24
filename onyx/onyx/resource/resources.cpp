#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"

namespace Onyx::Resources
{
Result<VKit::DeviceBuffer> CreateBuffer(VKit::DeviceBufferFlags p_Flags, VkDeviceSize p_InstanceSize,
                                        VkDeviceSize p_Capacity)
{
    return VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
        .SetSize(p_Capacity, p_InstanceSize)
        .Build();
}
} // namespace Onyx::Resources

// namespace Onyx::Detail
