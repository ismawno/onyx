#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/imgui/imgui.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;

void InitializeImGui(Window *p_Window);

void NewImGuiFrame();

void RenderImGuiData(ImDrawData *p_Data, VkCommandBuffer p_CommandBuffer);
void RenderImGuiWindows();

void ShutdownImGui();
} // namespace Onyx
