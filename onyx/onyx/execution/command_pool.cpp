#include "onyx/core/pch.hpp"
#include "onyx/execution/command_pool.hpp"

namespace Onyx
{
VkCommandBuffer CommandPool::Allocate()
{
    const auto result = Pool.Allocate();
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}
void CommandPool::Reset()
{
    VKIT_CHECK_EXPRESSION(Pool.Reset());
}
} // namespace Onyx
