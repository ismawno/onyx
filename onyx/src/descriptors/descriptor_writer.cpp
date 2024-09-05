#include "core/pch.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
DescriptorWriter::DescriptorWriter(const DescriptorSetLayout *p_Layout, const DescriptorPool *p_Pool) noexcept
    : m_Layout(p_Layout), m_Pool(p_Pool)
{
    m_Device = Core::GetDevice();
    m_Writes.reserve(m_Layout->BindingCount());
}

void DescriptorWriter::WriteBuffer(const u32 p_Binding, const VkDescriptorBufferInfo *p_BufferInfo) noexcept
{
    const VkDescriptorSetLayoutBinding &description = m_Layout->Binding(p_Binding);
    KIT_ASSERT(description.descriptorCount == 1,
               "This binding expects a single descriptor, but multiple were provided");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = description.descriptorType;
    write.dstBinding = p_Binding;
    write.pBufferInfo = p_BufferInfo;
    write.descriptorCount = 1;

    m_Writes.push_back(write);
}

void DescriptorWriter::WriteImage(const u32 p_Binding, const VkDescriptorImageInfo *p_ImageInfo) noexcept
{
    const VkDescriptorSetLayoutBinding &description = m_Layout->Binding(p_Binding);
    KIT_ASSERT(description.descriptorCount == 1,
               "This binding expects a single descriptor, but multiple were provided");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = description.descriptorType;
    write.dstBinding = p_Binding;
    write.pImageInfo = p_ImageInfo;
    write.descriptorCount = 1;

    m_Writes.push_back(write);
}

VkDescriptorSet DescriptorWriter::Build() noexcept
{
    const VkDescriptorSet set = m_Pool->Allocate(m_Layout->Layout());
    if (!set)
        return VK_NULL_HANDLE;
    Overwrite(set);
}

void DescriptorWriter::Overwrite(const VkDescriptorSet p_Set) noexcept
{
    for (VkWriteDescriptorSet &write : m_Writes)
        write.dstSet = p_Set;
    vkUpdateDescriptorSets(m_Device->VulkanDevice(), m_Writes.size(), m_Writes.data(), 0, nullptr);
}

} // namespace ONYX