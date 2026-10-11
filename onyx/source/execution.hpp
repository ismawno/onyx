#pragma once

#include "onyx/alias.hpp"
#include "onyx/core.hpp"
#include "graph/core.hpp"

namespace Onyx::Execution
{
void Initialize(const Specs &specs);
void Terminate();

Graph::CommandPool FindAvailableCommandPool(Graph::QueueType queue);

void MarkInUse(Graph::CommandPool pool, u64 inFlightValue);

Graph::CommandPool GetTransientGraphicsPool();
Graph::CommandPool GetTransientTransferPool();
} // namespace Onyx::Execution
