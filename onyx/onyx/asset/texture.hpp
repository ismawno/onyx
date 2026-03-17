#pragma once

#include "onyx/asset/handle.hpp"
#include "vkit/resource/device_image.hpp"

namespace Onyx
{
using Texture = Handle;
constexpr Texture NullTexture = NullHandle;

struct TextureData
{
    std::byte *Data = nullptr;
    u32 Width = 0;
    u32 Height = 0;
    u32 Components = 0;
    VkFormat Format = VK_FORMAT_UNDEFINED;

    usz ComputeSize() const
    {
        return Width * Height * VKit::DeviceImage::GetBytesPerPixel(Format);
    }
};

} // namespace Onyx
