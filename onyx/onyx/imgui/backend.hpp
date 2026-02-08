#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/imgui/imgui.hpp"
#include "onyx/core/core.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;
}

namespace Onyx::ImGuiBackend
{
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

ONYX_NO_DISCARD Result<> Create(Window *window);

void NewFrame();

ONYX_NO_DISCARD Result<> RenderData(ImDrawData *data, VkCommandBuffer commandBuffer);
void RenderWindows();

void Destroy();
} // namespace Onyx::ImGuiBackend
