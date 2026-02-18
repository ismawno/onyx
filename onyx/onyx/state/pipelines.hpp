#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/pipeline_layout.hpp"

namespace Onyx::Pipelines
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

const VKit::PipelineLayout &GetUnlitPipelineLayout();
template <Dimension D> const VKit::PipelineLayout &GetLitPipelineLayout();
template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(Shading shading);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateCirclePipeline(StencilPass pass,
                                                                    const VkPipelineRenderingCreateInfoKHR &renderInfo);

template <Dimension D>
ONYX_NO_DISCARD Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline(
    StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo);

} // namespace Onyx::Pipelines
