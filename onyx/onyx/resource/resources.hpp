#pragma once

#include "onyx/core/core.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/buffer.hpp"
#include "tkit/container/dynamic_array.hpp"

namespace Onyx::Resources
{
VKit::DeviceBuffer CreateBuffer(VKit::DeviceBufferFlags p_Flags, VkDeviceSize p_InstanceSize,
                                VkDeviceSize p_Capacity = ONYX_BUFFER_INITIAL_CAPACITY);

template <typename T>
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags p_Flags,
                                const VkDeviceSize p_Capacity = ONYX_BUFFER_INITIAL_CAPACITY)
{
    return CreateBuffer(p_Flags, sizeof(T), p_Capacity);
}

template <typename T>
VKit::DeviceBuffer CreateBuffer(const VKit::DeviceBufferFlags p_Flags, const TKit::DynamicArray<T> &p_Data)
{
    VKit::DeviceBuffer buffer = CreateBuffer(p_Flags, sizeof(T), p_Data.GetSize());
    if (buffer.GetInfo().Flags & VKit::DeviceBufferFlag_HostVisible)
        buffer.Write(p_Data.GetData(), {.size = p_Data.GetSize() * sizeof(T)});
    else
    {
        const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        VKIT_CHECK_EXPRESSION(buffer.UploadFromHost(Execution::GetTransientTransferPool(), queue->GetHandle(),
                                                    p_Data.GetData(), {.size = p_Data.GetSize() * sizeof(T)}));
    }
    return buffer;
}

template <typename T>
bool GrowBufferIfNeeded(VKit::DeviceBuffer &p_Buffer, const VkDeviceSize p_Instances,
                        const VKit::DeviceBufferFlags p_Flags, const f32 p_Factor = 1.5f)
{
    const VkDeviceSize inst = p_Buffer.GetInfo().InstanceCount;
    if (p_Buffer && p_Instances <= inst)
        return false;

    const VkDeviceSize ninst = static_cast<VkDeviceSize>(p_Factor * static_cast<f32>(p_Instances));
    p_Buffer.Destroy();
    p_Buffer = CreateBuffer<T>(p_Flags, ninst);
    return true;
}

} // namespace Onyx::Resources
