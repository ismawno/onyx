#pragma once

#include "onyx/core/alias.hpp"

ONYX_NAMESPACE_BEGIN

template <u32 N>
    requires(N == 2 || N == 3)
struct Dimension;

template <> struct Dimension<2>
{
};

template <> struct Dimension<3>
{
};

ONYX_NAMESPACE_END