#pragma once

#include "onyx/core/dimension.hpp"

namespace ONYX
{
struct ONYX_API Input
{
    static void PollEvents();

    Input() = delete;
};
} // namespace ONYX