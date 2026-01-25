#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/imgui/imgui.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;

void InitializeImGui(Window *window);

void NewImGuiFrame();

void RenderImGuiData(ImDrawData *data, VkCommandBuffer commandBuffer);
void RenderImGuiWindows();

void ShutdownImGui();
} // namespace Onyx
