#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Resources
{
VKit::DeviceBuffer CreateBuffer(VKit::DeviceBufferFlags p_Flags, VkDeviceSize p_InstanceSize, VkDeviceSize p_Capacity)
{
    const auto result = VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), p_Flags)
                            .SetSize(p_Capacity, p_InstanceSize)
                            .Build();
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}
} // namespace Onyx::Resources

// namespace Onyx::Detail
