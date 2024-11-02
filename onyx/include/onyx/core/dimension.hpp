#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/alias.hpp"

// This utility is used to provide 2 different APIs (2D and 3D) with as little code duplication as possible

namespace ONYX
{
enum Dimension : u8
{
    D2 = 2,
    D3 = 3
};
} // namespace ONYX
