#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx
{
using Asset = u32;
constexpr Asset NullAsset = TKit::Limits<Asset>::Max();

using AssetPool = u8;
constexpr AssetPool NullAssetPool = TKit::Limits<AssetPool>::Max();
} // namespace Onyx
