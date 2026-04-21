#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/imgui/imgui.hpp"
#include "onyx/core/core.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/platform/window.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;
}

namespace Onyx::ImGuiBackend
{
void Initialize();
void Terminate();

void Create(Window *window);

void NewFrame();

void RenderData(ImDrawData *data, VkCommandBuffer commandBuffer);
void UpdatePlatformWindows();

u32 GetPlatformWindowCount();

bool AcquirePlatformWindowImage(u32 windowIndex, Timeout timeout);
RenderSubmitInfo RenderPlatformWindow(u32 windowIndex, VKit::Queue *graphics,
                                                                        VkCommandBuffer cmd);
void PresentPlatformWindow(u32 windowIndex);

void Destroy();
} // namespace Onyx::ImGuiBackend
