#pragma once

#include "onyx/state/descriptor_set.hpp"
#include "onyx/core/core.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 32;
    u32 PoolSize = 1024;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &p_Specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout();
const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout();

ONYX_NO_DISCARD Result<VkDescriptorSet> WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info,
                                                                        VkDescriptorSet p_OldSet = VK_NULL_HANDLE);

ONYX_NO_DISCARD Result<DescriptorSet *> FindSuitableDescriptorSet(const VKit::DeviceBuffer &p_Buffer);

} // namespace Onyx::Descriptors
