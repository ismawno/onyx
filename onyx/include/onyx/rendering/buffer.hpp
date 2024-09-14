#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"

#include "kit/core/non_copyable.hpp"
#include "kit/memory/block_allocator.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API Buffer
{
    KIT_NON_COPYABLE(Buffer)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(Buffer, 32)

    struct Specs
    {
        VkDeviceSize InstanceCount;
        VkDeviceSize InstanceSize;
        VkBufferUsageFlags Usage;
        VkMemoryPropertyFlags Properties;
        VkDeviceSize MinimumAlignment = 1;
    };

    Buffer(const Specs &p_Specs) noexcept;

    ~Buffer() noexcept;

    void Map(VkDeviceSize p_Offset = 0, VkDeviceSize p_Size = VK_WHOLE_SIZE, VkMemoryMapFlags p_Flags = 0) noexcept;
    void Unmap() noexcept;

    void Write(const void *p_Data, VkDeviceSize p_Size = VK_WHOLE_SIZE, VkDeviceSize p_Offset = 0) noexcept;
    void WriteAt(usize p_Index, const void *p_Data) noexcept;

    void Flush(VkDeviceSize p_Size = VK_WHOLE_SIZE, VkDeviceSize p_Offset = 0) noexcept;
    void FlushAt(usize p_Index) noexcept;

    void Invalidate(VkDeviceSize p_Size = VK_WHOLE_SIZE, VkDeviceSize p_Offset = 0) noexcept;
    void InvalidateAt(usize p_Index) noexcept;

    VkDescriptorBufferInfo DescriptorInfo(VkDeviceSize p_Size = VK_WHOLE_SIZE,
                                          VkDeviceSize p_Offset = 0) const noexcept;
    VkDescriptorBufferInfo DescriptorInfoAt(usize p_Index) const noexcept;

    void *GetData() const noexcept;
    void *ReadAt(usize p_Index) const noexcept;

    void CopyFrom(const Buffer &p_Source) noexcept;

    VkBuffer GetBuffer() const noexcept;
    VkDeviceSize GetSize() const noexcept;

  private:
    void createBuffer(VkBufferUsageFlags p_Usage, VkMemoryPropertyFlags p_Properties) noexcept;

    KIT::Ref<Device> m_Device;
    void *m_Data = nullptr;

    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;

    VkDeviceSize m_InstanceSize;
    VkDeviceSize m_Size;
};
} // namespace ONYX