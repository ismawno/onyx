#pragma once

#include "onyx/core/core.hpp"
#include "onyx/platform/window.hpp"
#include "tkit/math/tensor.hpp"

#define ONYX_PLATFORM_ANY 0x00060000
#define ONYX_PLATFORM_WIN32 0x00060001
#define ONYX_PLATFORM_COCOA 0x00060002
#define ONYX_PLATFORM_WAYLAND 0x00060003
#define ONYX_PLATFORM_X11 0x00060004
#define ONYX_PLATFORM_NULL 0x00060005

#ifdef TKIT_OS_LINUX
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_X11
#elif defined(TKIT_OS_APPLE)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_COCOA
#elif defined(TKIT_OS_WINDOWS)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_WIN32
#endif

#ifndef ONYX_PLATFORM_AUTO
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_ANY
#endif

namespace Onyx::Platform
{
struct Specs
{
    u32 Platform = ONYX_PLATFORM_AUTO;

    // leaving these exposed but not very advisable to change
    VkSurfaceFormatKHR SurfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkFormat ColorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
};

void Initialize(const Specs &specs);
void Terminate();

VkSurfaceFormatKHR GetSurfaceFormat();
VkFormat GetColorFormat();
VkFormat GetDepthStencilFormat();

Window *CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(Window *window);
void DestroyWindows();
void PollEvents();

} // namespace Onyx::Platform
