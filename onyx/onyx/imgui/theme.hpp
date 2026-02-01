#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/core/alias.hpp"

namespace Onyx
{
enum Theme : u8
{
    Theme_Default,
    Theme_Cinder,
    Theme_Baby,
    Theme_DougBinks,
    Theme_LedSynthMaster,
    Theme_Hazel
};

void ApplyTheme(Theme theme);
} // namespace Onyx
