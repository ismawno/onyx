#pragma once

#include "onyx/state/descriptor_set.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 32;
    u32 PoolSize = 1024;
};
void Initialize(const Specs &p_Specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout();
const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout();

VkDescriptorSet WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                VkDescriptorSet p_OldSet = VK_NULL_HANDLE);

DescriptorSet &FindSuitableDescriptorSet(const VKit::DeviceBuffer &p_Buffer);

} // namespace Onyx::Descriptors
