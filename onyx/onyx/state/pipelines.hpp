#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/graphics_pipeline.hpp"

namespace Onyx::Pipelines
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

VkPipelineLayout GetGraphicsPipelineLayout(Shading p_Shading);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateCirclePipeline(
    StencilPass p_Pass, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline(
    StencilPass p_Pass, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

} // namespace Onyx::Pipelines
