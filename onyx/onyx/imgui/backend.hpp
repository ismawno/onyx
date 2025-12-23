#pragma once

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
