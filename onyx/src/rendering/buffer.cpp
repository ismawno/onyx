#include "core/pch.hpp"
#include "onyx/rendering/buffer.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
static VkDeviceSize alignedSize(const VkDeviceSize p_Size, const VkDeviceSize p_Alignment) noexcept
{
    return (p_Size + p_Alignment - 1) & ~(p_Alignment - 1);
}

static VkMappedMemoryRange mappedMemoryRange(const VkDeviceMemory p_Memory, const VkDeviceSize p_Offset,
                                             const VkDeviceSize p_Size) noexcept
{
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = p_Memory;
    range.offset = p_Offset;
    range.size = p_Size;
    return range;
}

Buffer::Buffer(const Specs &p_Specs) noexcept
    : m_InstanceSize(alignedSize(p_Specs.InstanceSize, p_Specs.MinimumAlignment)),
      m_Size(m_InstanceSize * p_Specs.InstanceCount)
{
    m_Device = Core::Device();
    createBuffer(p_Specs.Usage, p_Specs.Properties);
}

Buffer::~Buffer() noexcept
{
    if (m_Data)
        Unmap();
    vkDestroyBuffer(m_Device->VulkanDevice(), m_Buffer, nullptr);
    vkFreeMemory(m_Device->VulkanDevice(), m_BufferMemory, nullptr);
}

void Buffer::Map(const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const VkMemoryMapFlags p_Flags) noexcept
{
    if (m_Data)
        Unmap();
    KIT_ASSERT_RETURNS(vkMapMemory(m_Device->VulkanDevice(), m_BufferMemory, p_Offset, p_Size, p_Flags, &m_Data),
                       VK_SUCCESS, "Failed to map buffer memory");
}

void Buffer::Unmap() noexcept
{
    if (!m_Data)
        return;
    vkUnmapMemory(m_Device->VulkanDevice(), m_BufferMemory);
    m_Data = nullptr;
}

void Buffer::Write(const void *p_Data, const VkDeviceSize p_Size, const VkDeviceSize p_Offset) noexcept
{
    KIT_ASSERT(m_Data, "Cannot copy to unmapped buffer");
    KIT_ASSERT((p_Size == VK_WHOLE_SIZE && p_Offset == 0) || (p_Size != VK_WHOLE_SIZE && (p_Offset + p_Size) <= m_Size),
               "Size + offset must be lower than the buffer size");
    if (p_Size == VK_WHOLE_SIZE)
        std::memcpy(m_Data, p_Data, m_Size);
    else
    {
        std::byte *offsetted = static_cast<std::byte *>(m_Data) + p_Offset;
        std::memcpy(offsetted, p_Data, p_Size);
    }
}
void Buffer::WriteAt(const usize p_Index, const void *p_Data) noexcept
{
    Write(p_Data, m_InstanceSize, m_InstanceSize * p_Index);
}

void Buffer::Flush(const VkDeviceSize p_Size, const VkDeviceSize p_Offset) noexcept
{
    KIT_ASSERT(m_Data, "Cannot flush unmapped buffer");
    const VkMappedMemoryRange range = mappedMemoryRange(m_BufferMemory, p_Offset, p_Size);
    KIT_ASSERT_RETURNS(vkFlushMappedMemoryRanges(m_Device->VulkanDevice(), 1, &range), VK_SUCCESS,
                       "Failed to flush buffer memory");
}
void Buffer::FlushAt(const usize p_Index) noexcept
{
    Flush(m_InstanceSize, m_InstanceSize * p_Index);
}

void Buffer::Invalidate(const VkDeviceSize p_Size, const VkDeviceSize p_Offset) noexcept
{
    KIT_ASSERT(m_Data, "Cannot invalidate unmapped buffer");
    const VkMappedMemoryRange range = mappedMemoryRange(m_BufferMemory, p_Offset, p_Size);
    KIT_ASSERT_RETURNS(vkInvalidateMappedMemoryRanges(m_Device->VulkanDevice(), 1, &range), VK_SUCCESS,
                       "Failed to invalidate buffer memory");
}
void Buffer::InvalidateAt(const usize p_Index) noexcept
{
    Invalidate(m_InstanceSize, m_InstanceSize * p_Index);
}

VkDescriptorBufferInfo Buffer::DescriptorInfo(const VkDeviceSize p_Size, const VkDeviceSize p_Offset) const noexcept
{
    VkDescriptorBufferInfo info{};
    info.buffer = m_Buffer;
    info.offset = p_Offset;
    info.range = p_Size;
    return info;
}
VkDescriptorBufferInfo Buffer::DescriptorInfoAt(const usize p_Index) const noexcept
{
    return DescriptorInfo(m_InstanceSize, m_InstanceSize * p_Index);
}

void *Buffer::Data() const noexcept
{
    return m_Data;
}
void *Buffer::ReadAt(const usize p_Index) const noexcept
{
    return static_cast<std::byte *>(m_Data) + m_InstanceSize * p_Index;
}

void Buffer::CopyFrom(const Buffer &p_Source) noexcept
{
    KIT_ASSERT(m_Size == p_Source.m_Size, "Cannot copy buffers of different sizes");

    VkCommandBuffer commandBuffer = m_Device->BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = m_Size;
    vkCmdCopyBuffer(commandBuffer, p_Source.m_Buffer, m_Buffer, 1, &copyRegion);

    m_Device->EndSingleTimeCommands(commandBuffer);
}

VkBuffer Buffer::VulkanBuffer() const noexcept
{
    return m_Buffer;
}

VkDeviceSize Buffer::Size() const noexcept
{
    return m_Size;
}

void Buffer::createBuffer(const VkBufferUsageFlags p_Usage, const VkMemoryPropertyFlags p_Properties) noexcept
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_Size;
    bufferInfo.usage = p_Usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    KIT_ASSERT_RETURNS(vkCreateBuffer(m_Device->VulkanDevice(), &bufferInfo, nullptr, &m_Buffer), VK_SUCCESS,
                       "Failed to create buffer");

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(m_Device->VulkanDevice(), m_Buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = m_Device->FindMemoryType(requirements.memoryTypeBits, p_Properties);

    KIT_ASSERT_RETURNS(vkAllocateMemory(m_Device->VulkanDevice(), &allocInfo, nullptr, &m_BufferMemory), VK_SUCCESS,
                       "Failed to allocate buffer memory");
    vkBindBufferMemory(m_Device->VulkanDevice(), m_Buffer, m_BufferMemory, 0);
}

} // namespace ONYX