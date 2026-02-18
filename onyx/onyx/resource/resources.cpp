#include "onyx/core/pch.hpp"
#include "onyx/resource/resources.hpp"

namespace Onyx::Resources
{
Result<VKit::DeviceBuffer> CreateBuffer(const VKit::DeviceBufferFlags flags, const VkDeviceSize instanceSize,
                                        const VkDeviceSize capacity)
{
    return VKit::DeviceBuffer::Builder(Core::GetDevice(), Core::GetVulkanAllocator(), flags)
        .SetSize(capacity, instanceSize)
        .Build();
}

Result<TKit::Optional<VKit::DeviceBuffer>> CreateEnlargedBufferIfNeeded(const VKit::DeviceBuffer &buffer,
                                                                        const VkDeviceSize instances, const f32 factor)
{
    const VKit::DeviceBuffer::Info &info = buffer.GetInfo();
    const VkDeviceSize inst = info.InstanceCount;
    if (buffer && inst >= instances)
        return TKit::Optional<VKit::DeviceBuffer>::None();

    const VkDeviceSize ninst = static_cast<VkDeviceSize>(factor * static_cast<f32>(instances));
    const auto result = CreateBuffer(info.Flags, info.InstanceSize, ninst);
    TKIT_RETURN_ON_ERROR(result);
    return TKit::Optional<VKit::DeviceBuffer>::Some(result.GetValue());
}

Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const VkDeviceSize instances, const f32 factor)
{
    const VKit::DeviceBuffer::Info &info = buffer.GetInfo();
    const VkDeviceSize inst = info.InstanceCount;
    if (buffer && inst >= instances)
        return false;

    const VkDeviceSize ninst = static_cast<VkDeviceSize>(factor * static_cast<f32>(instances));
    const auto result = CreateBuffer(info.Flags, info.InstanceSize, ninst);
    TKIT_RETURN_ON_ERROR(result);
    buffer.Destroy();
    buffer = result.GetValue();
    return true;
}

} // namespace Onyx::Resources

// namespace Onyx::Detail
