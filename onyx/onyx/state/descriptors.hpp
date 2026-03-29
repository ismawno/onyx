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

const VKit::DescriptorSetLayout &GetUnlitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(Shading shading);

} // namespace Onyx::Descriptors
