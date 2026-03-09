#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/utils/limits.hpp"

namespace Onyx
{
using Texture = u32;
using Sampler = u32;
constexpr Texture NullTexture = TKit::Limits<Texture>::Max();
constexpr Sampler NullSampler = TKit::Limits<Sampler>::Max();

enum TextureType : u8
{
    Texture_Color,
    Texture_Linear
};

struct TextureData
{
    std::byte *Data = nullptr;
    u32 Width = 0;
    u32 Height = 0;
    u32 Channels = 0;
    TextureType Type = Texture_Color;

    u64 GetSize(const u32 bytesPerChannel = 1) const
    {
        return Width * Height * Channels * bytesPerChannel;
    }
};

} // namespace Onyx
