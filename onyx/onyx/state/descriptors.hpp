#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/descriptor_pool.hpp"

#define ONYX_MAX_SAMPLERS 8
#define ONYX_MAX_SAMPLED_IMAGES 1024
#define ONYX_MAX_ASSET_POOLS 255

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 256;
    u32 StorageBufferPoolSize = 2048;
    u32 SamplerPoolSize = 64;
    u32 SampledImagePoolSize = 4096;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();

const VKit::DescriptorSetLayout &GetUnlitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(Shading shading);

} // namespace Onyx::Descriptors
