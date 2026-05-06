#pragma once

#include "onyx/core.hpp"
#include "pass.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Descriptors
{
void Initialize(const Specs &specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorLayout(RenderPass pass);
const VKit::DescriptorSetLayout &GetRayMarchDescriptorLayout();
const VKit::DescriptorSetLayout &GetCompositorDescriptorLayout();
const VKit::DescriptorSetLayout &GetPostProcessDescriptorLayout();

template <Dimension D>
void BindBuffer(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorBufferInfo> info,
                RenderPass pass, u32 dstElement = 0);

template <Dimension D>
void BindImage(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorImageInfo> info,
               RenderPass pass, u32 dstElement = 0);

} // namespace Onyx::Descriptors
