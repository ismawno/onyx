#pragma once

#include "onyx/core/alias.hpp"
#include "vkit/resource/device_image.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx
{
using Texture = u32;
using Sampler = u32;
constexpr Texture NullTexture = TKit::Limits<Texture>::Max();
constexpr Sampler NullSampler = TKit::Limits<Sampler>::Max();

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
