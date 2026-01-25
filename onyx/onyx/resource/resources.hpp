#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/core/core.hpp"
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

template <typename T>
ONYX_NO_DISCARD Result<bool> GrowBufferIfNeeded(VKit::DeviceBuffer &buffer, const VkDeviceSize instances,
                                                const VKit::DeviceBufferFlags flags, const f32 factor = 1.5f)
{
    const VkDeviceSize inst = buffer.GetInfo().InstanceCount;
    if (buffer && instances <= inst)
        return false;

    const VkDeviceSize ninst = static_cast<VkDeviceSize>(factor * static_cast<f32>(instances));
    const auto result = CreateBuffer<T>(flags, ninst);
    TKIT_RETURN_ON_ERROR(result);

    buffer.Destroy();
    buffer = result.GetValue();
    return true;
}

} // namespace Onyx::Resources
