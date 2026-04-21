#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"

namespace Onyx::Resources
{
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags flags, const VkDeviceSize size)
{
    return ONYX_CHECK_EXPRESSION(
        VKit::DeviceBuffer::Builder(GetDevice(), GetVulkanAllocator(), flags).SetSize(size).Build());
}

bool GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const u32 instances, const u32 instanceSize, const f32 factor)
{
    const VKit::DeviceBuffer::Info &info = buffer.GetInfo();
    const u32 inst = u32(info.Size / instanceSize);
    if (buffer && inst >= instances)
        return false;

    const u32 ninst = GrowCapacity(instances, factor);
    buffer.Destroy();
    buffer = CreateBuffer(info.Flags, ninst * instanceSize);
    return true;
}

} // namespace Onyx::Resources
