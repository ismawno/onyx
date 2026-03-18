#pragma once

#include "tkit/utils/alias.hpp"
#include "tkit/utils/literals.hpp"

#define ONYX_MAX_WINDOWS 32

namespace Onyx
{
// Basically inherit all aliases from Toolkit
using namespace TKit::Alias;
using namespace TKit::Literals;
using ViewMask = u<ONYX_MAX_WINDOWS>;

} // namespace Onyx
