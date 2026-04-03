#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/descriptor_pool.hpp"

#define ONYX_MAX_SAMPLERS 8
#define ONYX_MAX_TEXTURES 1024

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 256;
    u32 StorageBufferPoolSize = 4096;
    u32 SamplerPoolSize = 128;
    u32 SampledImagePoolSize = 16384;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(DrawPass pass);

template <Dimension D>
void WriteBuffer(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorBufferInfo> info,
                 DrawPass pass, u32 dstElement = 0);

template <Dimension D>
void WriteImage(u32 binding, TKit::Span<const VkDescriptorSet> sets, TKit::Span<const VkDescriptorImageInfo> info,
                DrawPass pass, u32 dstElement = 0);

constexpr u32 GetInstancesBindingPoint()
{
    return 0;
}
template <Dimension D> constexpr u32 GetBoundsBindingPoint()
{
    return D == D2 ? 1 : 4;
}
constexpr u32 GetSamplersBindingPoint()
{
    return 2;
}
constexpr u32 GetTexturesBindingPoint()
{
    return 3;
}
template <Dimension D> constexpr u32 GetMaterialsBindingPoint()
{
    return D == D2 ? 4 : 5;
}
template <Dimension D> constexpr u32 GetPointLightsBindingPoint()
{
    return D == D2 ? 5 : 6;
}
constexpr u32 GetDirectionalLightsBindingPoint()
{
    return 7;
}

} // namespace Onyx::Descriptors
