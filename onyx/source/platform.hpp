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
constexpr VkFormat GetColorFormat()
{
    return VK_FORMAT_B8G8R8A8_SRGB;
}
constexpr VkFormat GetDepthStencilFormat()
{
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
}

} // namespace Onyx::Platform
