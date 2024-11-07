#pragma once

#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"

namespace ONYX
{
class ONYX_API DescriptorWriter
{
    KIT_NON_COPYABLE(DescriptorWriter)
  public:
    DescriptorWriter(const DescriptorSetLayout *p_Layout, const DescriptorPool *p_Pool) noexcept;

    void WriteBuffer(u32 p_Binding, const VkDescriptorBufferInfo *p_BufferInfo) noexcept;
    void WriteImage(u32 p_Binding, const VkDescriptorImageInfo *p_ImageInfo) noexcept;

    VkDescriptorSet Build() noexcept;
    void Overwrite(VkDescriptorSet p_Set) noexcept;

  private:
    KIT::Ref<Device> m_Device;
    DynamicArray<VkWriteDescriptorSet> m_Writes;
    const DescriptorSetLayout *m_Layout;
    const DescriptorPool *m_Pool;
};
} // namespace ONYX