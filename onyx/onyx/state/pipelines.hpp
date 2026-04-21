#pragma once

#include "onyx/core/core.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/rendering/pass.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/compute_pipeline.hpp"
#include "vkit/state/pipeline_layout.hpp"

namespace Onyx::Pipelines
{
void Initialize();
void Terminate();

template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(RenderPass pass);
const VKit::PipelineLayout &GetDistancePipelineLayout();
const VKit::PipelineLayout &GetCompositorPipelineLayout();

void ReloadShaders();

template <Dimension D>
VKit::GraphicsPipeline CreateGeometryPipeline(
    StencilPass pass, Geometry geo, const VkPipelineRenderingCreateInfoKHR &renderInfo);

template <Dimension D>
VKit::GraphicsPipeline CreateShadowPipeline(const Geometry geo, const VkFormat format);

// TODO(Isma): Revisit this name
VKit::ComputePipeline CreateDistancePipeline();

VKit::GraphicsPipeline CreateCompositorPipeline();

} // namespace Onyx::Pipelines
