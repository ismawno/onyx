#pragma once

#include "onyx/core/math.hpp"
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

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    TKit::Span<const CodePointRange> CharSets = FontCharSet_ASCII;
    u32v2 GlyphSize{32, 32};
    f32 SDFRange = 4.f;
    f32 Padding = 2.f;
    u32 AtlasWidth = 512;
};
ONYX_NO_DISCARD Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts = {});
#endif
} // namespace Onyx
