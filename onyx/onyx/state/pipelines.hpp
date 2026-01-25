#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/graphics_pipeline.hpp"

namespace Onyx::Pipelines
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

VkPipelineLayout GetGraphicsPipelineLayout(Shading shading);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateCirclePipeline(StencilPass pass,
                                                                    const VkPipelineRenderingCreateInfoKHR &renderInfo);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline(
    StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo);

} // namespace Onyx::Pipelines
