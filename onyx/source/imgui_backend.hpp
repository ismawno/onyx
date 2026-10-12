#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/core.hpp"
#include "onyx/window.hpp"
#include "renderer.hpp"
#include <imgui.h>
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;
void ImGuiBackend_Initialize();
void ImGuiBackend_Terminate();

void ImGuiBackend_Create(Window *window);

void ImGuiBackend_NewFrame();

void ImGuiBackend_RenderData(ImDrawData *data, Graph::CommandBuffer cmd);
void ImGuiBackend_UpdatePlatformWindows();

u32 ImGuiBackend_GetPlatformWindowCount();

bool ImGuiBackend_AcquirePlatformWindowImage(u32 windowIndex, u64 timeout);
RenderSubmitInfo ImGuiBackend_RenderPlatformWindow(u32 windowIndex, Graph::CommandBuffer cmd);
void ImGuiBackend_PresentPlatformWindow(u32 windowIndex);

void ImGuiBackend_DestroyCurrent();
} // namespace Onyx
