#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"

// This is a utility class used to provide 2 different APIs (2D and 3D) with as little code duplication as possible

#define ONYX_DIMENSION_TEMPLATE                                                                                        \
    template <u32 N>                                                                                                   \
        requires(N == 2 || N == 3)

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Dimension;

template <> struct ONYX_API Dimension<2>
{
    static constexpr u32 N = 2;
};

template <> struct ONYX_API Dimension<3>
{
    static constexpr u32 N = 3;
};
} // namespace ONYX