#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/core/core.hpp"
#include "vkit/resource/sampler.hpp"
#include "tkit/container/dynamic_array.hpp"

namespace Onyx::Resources
{
ONYX_NO_DISCARD Result<VKit::DeviceBuffer> CreateBuffer(VKit::DeviceBufferFlags flags, VkDeviceSize instanceSize,
                                                        VkDeviceSize capacity = ONYX_BUFFER_INITIAL_CAPACITY);

template <typename T>
ONYX_NO_DISCARD Result<VKit::DeviceBuffer> CreateBuffer(const VKit::DeviceBufferFlags flags,
                                                        const VkDeviceSize capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    return CreateBuffer(flags, sizeof(T), capacity);
}

ONYX_NO_DISCARD Result<VKit::Sampler> CreateDefaultSampler();

template <typename T>
ONYX_NO_DISCARD Result<VKit::DeviceBuffer> CreateBuffer(const VKit::DeviceBufferFlags flags,
                                                        const TKit::DynamicArray<T> &data)
{
    const auto bresult = CreateBuffer(flags, sizeof(T), data.GetSize());
    TKIT_RETURN_ON_ERROR(bresult);
    VKit::DeviceBuffer &buffer = bresult.GetValue();

    if (buffer.GetInfo().Flags & VKit::DeviceBufferFlag_HostVisible)
        buffer.Write(data.GetData(), {.size = data.GetSize() * sizeof(T)});
    else
    {
        const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        const auto uresult = buffer.UploadFromHost(Execution::GetTransientTransferPool(), queue->GetHandle(),
                                                   data.GetData(), {.size = data.GetSize() * sizeof(T)});
        if (!uresult)
        {
            buffer.Destroy();
            return uresult;
        }
    }
    return buffer;
}

inline VkDeviceSize GrowCapacity(const VkDeviceSize instances, const f32 factor = 1.5f)
{
    return VkDeviceSize(factor * f32(instances));
}

Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, VkDeviceSize instances, const f32 factor = 1.5f);

} // namespace Onyx::Resources
