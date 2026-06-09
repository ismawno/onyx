#pragma once

#include "onyx/instance.hpp"
#include "pass.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/compute_pipeline.hpp"
#include "vkit/state/pipeline_layout.hpp"

namespace Onyx::Pipelines
{
void Initialize();
void Terminate();

template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(RenderPass pass);
const VKit::PipelineLayout &GetPipelineLayout(StandalonePass pass);

// TODO(Isma): Make this public?
void ReloadShaders();

template <Dimension D> VKit::GraphicsPipeline CreateGeometryPipeline(PipelinePass pass, BlendPass bpass, Geometry geo);
template <Dimension D> VKit::GraphicsPipeline CreateShadowPipeline(const Geometry geo, const VkFormat format);

VKit::ComputePipeline CreateRayMarchPipeline();
VKit::GraphicsPipeline CreateBlendPipeline();
VKit::GraphicsPipeline CreatePostProcessPipeline();
VKit::GraphicsPipeline CreateCompositorPipeline();

} // namespace Onyx::Pipelines
