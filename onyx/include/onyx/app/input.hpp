#pragma once

#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Input
{
    static void PollEvents();
};

using Input2 = Input<2>;
using Input3 = Input<3>;
} // namespace ONYX