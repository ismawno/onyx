#pragma once

#include "onyx/core/alias.hpp"
#include "vkit/execution/command_pool.hpp"

namespace Onyx::Queues
{
void Initialize();
void Terminate();

u32 GetFamilyIndex(VKit::QueueType p_Type);
bool IsSeparateTransferMode();

VKit::Queue *GetQueue(VKit::QueueType p_Type);

VKit::CommandPool &GetGraphicsPool();
VKit::CommandPool &GetTransferPool();

} // namespace Onyx::Queues
