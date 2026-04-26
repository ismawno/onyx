#pragma once

#include "onyx/core/core.hpp"
#include "onyx/rendering/pass.hpp"
#include "vkit/state/descriptor_pool.hpp"

#define ONYX_MAX_SAMPLERS 8
#define ONYX_MAX_TEXTURES 1024
#define ONYX_MAX_TEXTURE_MAPS 32

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

// these are funcs bc maybe some binding points will depend on dim or pass in the future
//  TODO(Isma): Have to add one for stencil (outlines)
constexpr u32 GetCompositorColorAttachmentsBindingPoint()
{
    return 0;
}
constexpr u32 GetPostProcessColorAttachmentsBindingPoint()
{
    return 0;
}
constexpr u32 GetPostProcessOutlineAttachmentsBindingPoint()
{
    return 1;
}
constexpr u32 GetPostProcessStencilAttachmentsBindingPoint()
{
    return 2;
}

constexpr u32 GetInstancesBindingPoint()
{
    return 0;
}
constexpr u32 GetBoundsBindingPoint()
{
    return 3;
}
constexpr u32 GetSamplersBindingPoint()
{
    return 1;
}
constexpr u32 GetTexturesBindingPoint()
{
    return 2;
}
constexpr u32 GetMaterialsBindingPoint()
{
    return 4;
}
constexpr u32 GetPointLightsBindingPoint()
{
    return 5;
}
constexpr u32 GetDirectionalLightsBindingPoint()
{
    return 9;
}

constexpr u32 GetShadowSamplerBindingPoint()
{
    return 6;
}

constexpr u32 GetShadowMapsBindingPoint()
{
    return 7;
}

constexpr u32 GetPointMapsBindingPoint()
{
    return 7;
}
constexpr u32 GetDirectionalMapsBindingPoint()
{
    return 8;
}

constexpr u32 GetOcclusionMapBindingPoint()
{
    return 0;
}
constexpr u32 GetDistanceMapBindingPoint()
{
    return 1;
}

} // namespace Onyx::Descriptors
