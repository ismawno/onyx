#pragma once

#include "onyx/core/api.hpp"
#include "onyx/state/state.hpp"
#include "vkit/state/graphics_pipeline.hpp"

namespace Onyx::Pipelines
{
ONYX_API void Initialize();
ONYX_API void Terminate();

ONYX_API VkPipelineLayout GetGraphicsPipelineLayout(Shading p_Shading);

template <Dimension D>
VKit::GraphicsPipeline CreateStaticMeshPipeline(PipelineMode p_Mode,
                                                const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template <Dimension D>
VKit::GraphicsPipeline CreateCirclePipeline(PipelineMode p_Mode, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

} // namespace Onyx::Pipelines
