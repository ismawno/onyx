#pragma once

#include "onyx/definitions.hpp"
#include "tkit/utils/alias.hpp"
#include "tkit/utils/literals.hpp"
#include "tkit/utils/bit.hpp"

namespace Onyx
{
// Basically inherit all aliases from Toolkit
using namespace TKit::Alias;
using namespace TKit::Literals;
using ViewMask = u<TKit::NextPowerOfTwo(ONYX_MAX_VIEWS)>;
using CodePoint = u32;

} // namespace Onyx
