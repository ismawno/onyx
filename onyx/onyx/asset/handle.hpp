#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx
{
using Handle = u32;
constexpr Handle NullHandle = TKit::Limits<Handle>::Max();
} // namespace Onyx
