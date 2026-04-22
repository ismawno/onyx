#pragma once

#include "onyx/core/math.hpp"
#include "onyx/asset/image.hpp"

namespace Onyx
{
struct GlyphData
{
    f32v2 Min{0.f};
    f32v2 Max{0.f};
    f32v2 MinTexCoord{0.f};
    f32v2 MaxTexCoord{0.f};
    f32 Advance = 0.f;
};

struct GlyphKerning
{
    u64 Key;
    f32 Kerning;
};

struct Glyph
{
    u32 Id;
    f32 Advance;
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
    TKit::TierArray<GlyphKerning> Kerning{};
    ImageData AtlasData{};
    f32 Ascender = 0.f;
    f32 Descender = 0.f;
    f32 LineHeight = 0.f;

    f32 GetKerning(u32 code0, u32 code1) const;
};

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    TKit::Span<const CodePointRange> CharSets = FontCharSet_ASCII;
    f32 Padding = 2.f;
    f32 SDFRange = 4.f;
    f32 FontScale = 1.f;
    f32 MaxCornerAngle = 3.f;
    f32 EmSize = 40.f;
};
ONYX_NO_DISCARD Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts = {});
#endif
} // namespace Onyx
