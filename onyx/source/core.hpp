#pragma once

#include "onyx/core.hpp"
#include "onyx/window.hpp"
#include "pass.hpp"
#include "graph/core.hpp"

namespace Onyx
{
Window *CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(const Window *window);
void DestroyWindows();

template <Dimension D> Graph::DescriptorSetLayout GetDescriptorLayout(RenderPass pass);
Graph::DescriptorSetLayout GetDescriptorLayout(StandalonePass pass);

Graph::CommandPool FindAvailableCommandPool();

void MarkCommandPoolInUse(Graph::CommandPool pool, u64 inFlightValue);

Graph::CommandPool GetTransientGraphicsCommandPool();

}; // namespace Onyx
