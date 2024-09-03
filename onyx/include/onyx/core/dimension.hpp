#pragma once

#include "onyx/core/api.hpp"

// This utility is used to provide 2 different APIs (2D and 3D) with as little code duplication as possible

#define ONYX_DIMENSION_TEMPLATE                                                                                        \
    template <u32 N>                                                                                                   \
        requires(N == 2 || N == 3)
