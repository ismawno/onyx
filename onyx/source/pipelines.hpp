#pragma once

#include "onyx/instance.hpp"
#include "pass.hpp"
#include "graph/core.hpp"

namespace Onyx::Pipelines
{
void Initialize();
void Terminate();

template <Dimension D> Graph::PipelineLayout GetPipelineLayout(RenderPass pass);
Graph::PipelineLayout GetPipelineLayout(StandalonePass pass);

// TODO(Isma): Make this public?
void ReloadShaders();

template <Dimension D> Graph::Pipeline CreateGeometryPipeline(PipelinePass pass, BlendPass bpass, Geometry geo);
template <Dimension D> Graph::Pipeline CreateShadowPipeline(const Geometry geo, const VkFormat format);

Graph::Pipeline CreateRayMarchPipeline();
Graph::Pipeline CreateBlendPipeline();
Graph::Pipeline CreatePostProcessPipeline();
Graph::Pipeline CreateCompositorPipeline();

} // namespace Onyx::Pipelines
