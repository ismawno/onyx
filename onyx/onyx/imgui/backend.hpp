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
ONYX_NO_DISCARD Result<> Initialize();
void Terminate();

ONYX_NO_DISCARD Result<> Create(Window *window);

void NewFrame();

ONYX_NO_DISCARD Result<> RenderData(ImDrawData *data, VkCommandBuffer commandBuffer);
ONYX_NO_DISCARD Result<> UpdatePlatformWindows();

u32 GetPlatformWindowCount();

ONYX_NO_DISCARD Result<bool> AcquirePlatformWindowImage(u32 windowIndex, Timeout timeout = Block);
ONYX_NO_DISCARD Result<Renderer::RenderSubmitInfo> RenderPlatformWindow(u32 windowIndex, VKit::Queue *graphics,
                                                                        VkCommandBuffer cmd);
ONYX_NO_DISCARD Result<> PresentPlatformWindow(u32 windowIndex);

void Destroy();
} // namespace Onyx::ImGuiBackend
