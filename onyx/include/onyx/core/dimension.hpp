#pragma once

#include "onyx/core/alias.hpp"

#define ONYX_DIMENSION_TEMPLATE                                                                                        \
    template <u32 N>                                                                                                   \
        requires(N == 2 || N == 3)

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE struct Dimension;

template <> struct Dimension<2>
{
    static constexpr u32 N = 2;
};

template <> struct Dimension<3>
{
    static constexpr u32 N = 3;
};

ONYX_NAMESPACE_END