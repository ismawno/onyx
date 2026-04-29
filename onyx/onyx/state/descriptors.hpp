#pragma once

#include "onyx/core/core.hpp"
#include "onyx/rendering/pass.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 1 << 10;
    u32 StorageBufferPoolSize = 1 << 14;
    u32 SamplerPoolSize = 1 << 10;
    u32 SampledImagePoolSize = 1 << 17;
    u32 CombinedImageSamplerPoolSize = 1 << 10;
    u32 StorageImagePoolSize = 1 << 9;
};
void Initialize(const Specs &specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorLayout(RenderPass pass);
const VKit::DescriptorSetLayout &GetDistanceDescriptorLayout();
const VKit::DescriptorSetLayout &GetCompositorDescriptorLayout();
const VKit::DescriptorSetLayout &GetPostProcessDescriptorLayout();

template <Dimension D>
void BindBuffer(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorBufferInfo> info,
                RenderPass pass, u32 dstElement = 0);

template <Dimension D>
void BindImage(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorImageInfo> info,
               RenderPass pass, u32 dstElement = 0);

} // namespace Onyx::Descriptors
