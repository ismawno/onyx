#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"
#include "kit/container/static_array.hpp"

#include <vulkan/vulkan.hpp>
#include <span>

namespace ONYX
{
class ONYX_API DescriptorPool : public KIT::RefCounted<DescriptorPool>
{
    KIT_NON_COPYABLE(DescriptorPool)
  public:
    struct Specs
    {
        u32 MaxSets;
        std::span<const VkDescriptorPoolSize> PoolSizes;
        VkDescriptorPoolCreateFlags PoolFlags = 0;
    };

    DescriptorPool(const Specs &p_Specs) noexcept;
    ~DescriptorPool() noexcept;

    VkDescriptorSet Allocate(VkDescriptorSetLayout p_Layout) const noexcept;

    void Deallocate(std::span<const VkDescriptorSet> p_Sets) const noexcept;
    void Deallocate(VkDescriptorSet p_Set) const noexcept;
    void Reset() noexcept;

  private:
    KIT::Ref<Device> m_Device;
    VkDescriptorPool m_Pool;

    // consider a pool per thread?
    mutable std::mutex m_Mutex;
};
} // namespace ONYX