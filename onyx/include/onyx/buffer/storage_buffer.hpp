#pragma once

#include "onyx/buffer/buffer.hpp"
#include "onyx/core/core.hpp"
#include "onyx/core/glm.hpp"
#include "kit/container/storage.hpp"

namespace ONYX
{
template <typename T> class StorageBuffer : public Buffer
{
    KIT_NON_COPYABLE(StorageBuffer)
  public:
    StorageBuffer(const std::span<const T> p_Data) noexcept : Buffer(createBufferSpecs(p_Data))
    {
        Map();
        Write(p_Data.data());
        Flush();
    }

  private:
    static Buffer::Specs createBufferSpecs(const std::span<const T> p_Data) noexcept
    {
        const auto &props = Core::GetDevice()->GetProperties();
        Buffer::Specs specs{};
        specs.InstanceCount = p_Data.size();
        specs.InstanceSize = sizeof(T);
        specs.Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        specs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        specs.MinimumAlignment =
            glm::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);
        return specs;
    }
};
} // namespace ONYX