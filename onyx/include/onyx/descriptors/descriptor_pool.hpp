#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"

#include <vulkan/vulkan.hpp>
#include <span>

namespace ONYX
{
class ONYX_API DescriptorPool
{
    KIT_NON_COPYABLE(DescriptorPool)
  public:
    DescriptorPool(u32 p_MaxSets, std::span<const VkDescriptorPoolSize> p_PoolSizes,
                   VkDescriptorPoolCreateFlags p_PoolFlags) noexcept;
    ~DescriptorPool() noexcept;

    VkDescriptorSet Allocate(VkDescriptorSetLayout p_Layout) const noexcept;

    void Deallocate(std::span<const VkDescriptorSet> p_Sets) const noexcept;
    void Deallocate(VkDescriptorSet p_Set) const noexcept;
    void Reset() noexcept;

  private:
    KIT::Ref<Device> m_Device;
    VkDescriptorPool m_Pool;
};
} // namespace ONYX