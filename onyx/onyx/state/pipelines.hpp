#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/pipeline_layout.hpp"

namespace Onyx::Pipelines
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

const VKit::PipelineLayout &GetStencilPipelineLayout();
template <Dimension D> const VKit::PipelineLayout &GetFillPipelineLayout();
template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(DrawPass pass);

ONYX_NO_DISCARD Result<> ReloadShaders();

template <Dimension D>
Result<VKit::GraphicsPipeline> CreatePipeline(StencilPass pass, Geometry geo,
                                              const VkPipelineRenderingCreateInfoKHR &renderInfo);

} // namespace Onyx::Pipelines
