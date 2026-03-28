#pragma once

#include "onyx/asset/texture.hpp"

namespace Onyx
{

struct GlyphData
{
    f32v2 MinTexCoord{0.f};
    f32v2 MaxTexCoord{0.f};
    f32v2 Bearing{0.f};
    f32v2 Size{0.f};
    f32 Advance = 0.f;
};

struct CodePointRange
{
    u32 First = 0;
    u32 Last = 0;
};

constexpr CodePointRange FontCharSet_ASCII = CodePointRange{32, 126};

struct FontData
{
    TKit::TierArray<CodePointRange> CodePoints{};
    TKit::TierArray<GlyphData> Glyphs{};
    TextureData AtlasData{};
    u32 CharSetCount = 0;
    f32 Ascender = 0.f;
    f32 Descender = 0.f;
    f32 LineHeight = 0.f;
};
} // namespace Onyx
