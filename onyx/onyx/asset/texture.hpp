#pragma once

#include "onyx/core/alias.hpp"

namespace Onyx
{
using Texture = u32;
constexpr Texture NullTexture = TKit::Limits<Texture>::Max();
} // namespace Onyx
