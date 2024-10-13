#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"

#include <vulkan/vulkan.hpp>
#include <span>

namespace ONYX
{
class ONYX_API DescriptorSetLayout : public KIT::RefCounted<DescriptorSetLayout>
{
    KIT_NON_COPYABLE(DescriptorSetLayout)
  public:
    DescriptorSetLayout(std::span<const VkDescriptorSetLayoutBinding> p_Bindings) noexcept;
    ~DescriptorSetLayout() noexcept;

    static VkDescriptorSetLayoutBinding CreateBinding(u32 p_Binding, VkDescriptorType p_DescriptorType,
                                                      VkShaderStageFlags p_StageFlags, u32 p_Count = 1) noexcept;

    const VkDescriptorSetLayoutBinding &GetBinding(usize p_Index) const noexcept;
    VkDescriptorSetLayout GetLayout() const noexcept;

    usize GetBindingCount() const noexcept;

  private:
    KIT::Ref<Device> m_Device;
    VkDescriptorSetLayout m_Layout;
    DynamicArray<VkDescriptorSetLayoutBinding> m_Bindings;
};
} // namespace ONYX