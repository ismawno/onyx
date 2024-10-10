#pragma once

#include "onyx/buffer/buffer.hpp"
#include "kit/container/storage.hpp"

namespace ONYX
{
template <typename T> class DeviceBuffer
{
    KIT_NON_COPYABLE(DeviceBuffer)
  public:
    DeviceBuffer(const std::span<const T> p_Data, const VkBufferUsageFlags p_Usage) noexcept
    {
        Buffer::Specs specs{};
        specs.InstanceCount = p_Data.size();
        specs.InstanceSize = sizeof(T);
        specs.Usage = p_Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        m_Buffer.Create(specs);

        Buffer::Specs stagingSpecs = specs;
        stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        Buffer stagingBuffer(stagingSpecs);
        stagingBuffer.Map();
        stagingBuffer.Write(p_Indices.data());
        stagingBuffer.Flush();
        stagingBuffer.Unmap();

        m_Buffer->CopyFrom(stagingBuffer);
    }
    ~DeviceBuffer() noexcept
    {
        m_Buffer.Destroy();
    }

    VkBuffer GetBuffer() const noexcept
    {
        return m_Buffer->GetBuffer();
    }
    VkDeviceSize GetSize() const noexcept
    {
        return m_Buffer->GetInstanceCount();
    }

  private:
    KIT::Storage<Buffer> m_Buffer;
};
} // namespace ONYX