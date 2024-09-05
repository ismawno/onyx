#include "core/pch.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
DescriptorPool::DescriptorPool(const u32 p_MaxSets, const std::span<const VkDescriptorPoolSize> p_PoolSizes,
                               const VkDescriptorPoolCreateFlags p_PoolFlags) noexcept
{
    m_Device = Core::GetDevice();
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(p_PoolSizes.size());
    poolInfo.pPoolSizes = p_PoolSizes.data();
    poolInfo.maxSets = p_MaxSets;
    poolInfo.flags = p_PoolFlags;

    KIT_ASSERT_RETURNS(vkCreateDescriptorPool(m_Device->VulkanDevice(), &poolInfo, nullptr, &m_Pool), VK_SUCCESS,
                       "Failed to create descriptor pool");
}

DescriptorPool::~DescriptorPool() noexcept
{
    vkDestroyDescriptorPool(m_Device->VulkanDevice(), m_Pool, nullptr);
}

VkDescriptorSet DescriptorPool::Allocate(const VkDescriptorSetLayout p_Layout) const noexcept
{
    VkDescriptorSet set;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &p_Layout;

    if (vkAllocateDescriptorSets(m_Device->VulkanDevice(), &allocInfo, &set) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    return set;
}

void DescriptorPool::Deallocate(const std::span<const VkDescriptorSet> p_Sets) const noexcept
{
    vkFreeDescriptorSets(m_Device->VulkanDevice(), m_Pool, static_cast<u32>(p_Sets.size()), p_Sets.data());
}

void DescriptorPool::Deallocate(const VkDescriptorSet p_Set) const noexcept
{
    vkFreeDescriptorSets(m_Device->VulkanDevice(), m_Pool, 1, &p_Set);
}

void DescriptorPool::Reset() noexcept
{
    vkResetDescriptorPool(m_Device->VulkanDevice(), m_Pool, 0);
}

} // namespace ONYX