#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"

// This utility is used to provide 2 different APIs (2D and 3D) with as little code duplication as possible

namespace ONYX
{
template <u32 N> constexpr bool IsDim() noexcept
{
    return N == 2 || N == 3;
}
} // namespace ONYX
