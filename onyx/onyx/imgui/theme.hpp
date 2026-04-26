#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/core/alias.hpp"

namespace Onyx
{
enum ImGuiTheme : u8
{
    ImGuiTheme_Default,
    ImGuiTheme_Cinder,
    ImGuiTheme_Baby,
    ImGuiTheme_DougBinks,
    ImGuiTheme_LedSynthMaster,
    ImGuiTheme_Hazel
};

void ApplyImGuiTheme(ImGuiTheme theme);
} // namespace Onyx
