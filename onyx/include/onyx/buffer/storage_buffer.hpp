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
    StorageBuffer(const std::span<const T> p_Data) noexcept : Buffer(createBufferSpecs(p_Data.size()))
    {
        Map();
        // Cant use a plain write because of alignment issues
        for (usize i = 0; i < p_Data.size(); ++i)
            WriteAt(i, &p_Data[i]);
        Flush();
    }

    StorageBuffer(const usize p_Size) noexcept : Buffer(createBufferSpecs(p_Size))
    {
        Map();
    }

  private:
    static Buffer::Specs createBufferSpecs(const usize p_Size) noexcept
    {
        const auto &props = Core::GetDevice()->GetProperties();
        Buffer::Specs specs{};
        specs.InstanceCount = p_Size;
        specs.InstanceSize = sizeof(T);
        specs.Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        specs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        specs.AllocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        specs.MinimumAlignment =
            glm::max(props.limits.minStorageBufferOffsetAlignment, props.limits.nonCoherentAtomSize);
        return specs;
    }
};
} // namespace ONYX