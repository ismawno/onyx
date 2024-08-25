#pragma once

#include "onyx/core/dimension.hpp"

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE struct Input
{
    static void PollEvents();
};

using Input2 = Input<2>;
using Input3 = Input<3>;

ONYX_NAMESPACE_END