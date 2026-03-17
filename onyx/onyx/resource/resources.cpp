#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"

namespace Onyx::Resources
{
Result<VKit::DeviceBuffer> CreateBuffer(const VKit::DeviceBufferFlags flags, const VkDeviceSize size)
{
    return VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), flags).SetSize(size).Build();
}

Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const u32 instances, const u32 instanceSize,
                                const f32 factor)
{
    const VKit::DeviceBuffer::Info &info = buffer.GetInfo();
    const u32 inst = u32(info.Size / instanceSize);
    if (buffer && inst >= instances)
        return false;

    const u32 ninst = GrowCapacity(instances, factor);
    const auto result = CreateBuffer(info.Flags, ninst * instanceSize);
    TKIT_RETURN_ON_ERROR(result);
    buffer.Destroy();
    buffer = result.GetValue();
    return true;
}

} // namespace Onyx::Resources
