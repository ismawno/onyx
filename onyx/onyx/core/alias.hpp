#pragma once

#include "tkit/utils/alias.hpp"
#include "tkit/utils/literals.hpp"

#define ONYX_MAX_VIEWS 32
#define ONYX_MAX_ATTACHMENTS (5 * ONYX_MAX_VIEWS)

namespace Onyx
{
// Basically inherit all aliases from Toolkit
using namespace TKit::Alias;
using namespace TKit::Literals;
using ViewMask = u<ONYX_MAX_VIEWS>;

} // namespace Onyx
