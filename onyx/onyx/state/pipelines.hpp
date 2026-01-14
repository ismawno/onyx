#pragma once

#include "onyx/property/instance.hpp"
#include "vkit/state/graphics_pipeline.hpp"

namespace Onyx::Pipelines
{
void Initialize();
void Terminate();

VkPipelineLayout GetGraphicsPipelineLayout(Shading p_Shading);

template <Dimension D>
VKit::GraphicsPipeline CreateStaticMeshPipeline(StencilPass p_Pass,
                                                const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template <Dimension D>
VKit::GraphicsPipeline CreateCirclePipeline(StencilPass p_Pass, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

} // namespace Onyx::Pipelines
