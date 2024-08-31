#pragma once

#include "onyx/core/dimension.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Window;
struct ONYX_API Input
{
    static void PollEvents();

    ONYX_DIMENSION_TEMPLATE static void InstallCallbacks(const Window<N> &p_Window) noexcept;

    Input() = delete;
};
} // namespace ONYX