#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/descriptor_pool.hpp"

namespace Onyx::Descriptors
{
struct Specs
{
    u32 MaxSets = 256;
    u32 PoolSize = 1024;
};
ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

const VKit::DescriptorPool &GetDescriptorPool();

const VKit::DescriptorSetLayout &GetUnlitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout();
template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(Shading shading);

} // namespace Onyx::Descriptors
