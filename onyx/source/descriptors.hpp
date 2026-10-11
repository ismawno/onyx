#pragma once

#include "pass.hpp"
#include "graph/core.hpp"

namespace Onyx::Descriptors
{
void Initialize();
void Terminate();

template <Dimension D> Graph::DescriptorSetLayout GetDescriptorLayout(RenderPass pass);
Graph::DescriptorSetLayout GetDescriptorLayout(StandalonePass pass);
} // namespace Onyx::Descriptors
