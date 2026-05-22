#pragma once

#include "onyx/platform.hpp"

namespace Onyx::Platform
{
void Initialize(const Specs &specs);
void Terminate();
void PollEvents();

constexpr VkSurfaceFormatKHR GetSurfaceFormat()
{
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
}
} // namespace Onyx::Platform
