#pragma once

#include "onyx/math.hpp"
#include "onyx/image.hpp"
#include "tkit/container/hash_set.hpp"
#include "tkit/container/hash_map.hpp"

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

struct Glyph
{
    u32 Id;
    f32 Advance;
};

struct CodePointRange
{
    u32 First;
    u32 Last;
};

enum FontCharSet : u32
{
    CharSet_ASCII,
    CharSet_Latin1Supplement,
    CharSet_LatinExtendedA,
    CharSet_LatinExtendedB,
    CharSet_Greek,
    CharSet_Cyrillic,
    CharSet_Arrows,
    CharSet_MathOperators,
    CharSet_MiscTechnical,
    CharSet_EnclosedAlphanumerics,
    CharSet_BoxDrawing,
    CharSet_BlockElements,
    CharSet_GeometricShapes,
    CharSet_MiscSymbols,
    CharSet_Dingbats,
    CharSet_MiscSymbolsAndArrows,
    CharSet_BraillePatterns,
    CharSet_ControlPictures,
    CharSet_OCR,
    CharSet_YijingHexagrams,
    CharSet_EnclosedAlphanumSupplement,
    CharSet_MiscSymbolsAndPictographs,
    CharSet_AlchemicalSymbols,
    CharSet_TransportAndMap,
    CharSet_GeometricShapesExtended,
    CharSet_SupplementalArrowsC,
    CharSet_OrnamentalDingbats,
    CharSet_ChessSymbols,
    CharSet_PlayingCards,
    CharSet_DominoTiles,
    CharSet_MahjongTiles,
    CharSet_TaiXuanJing,
    CharSet_AncientSymbols,
    CharSet_PhaistosDisc,
    CharSet_LegacyComputing,
    CharSet_SymbolsPictographsExtA,
    CharSet_Count
};

constexpr TKit::FixedArray<CodePointRange, CharSet_Count> CharSets = {
    {32, 126},          {160, 255},         {256, 383},         {384, 591},         {880, 1023},
    {1024, 1279},       {8592, 8703},       {8704, 8959},       {8960, 9215},       {9312, 9471},
    {9472, 9599},       {9600, 9631},       {9632, 9727},       {9728, 9983},       {9984, 10175},
    {11008, 11263},     {10240, 10495},     {9216, 9279},       {9280, 9311},       {19904, 19967},
    {0x1F100, 0x1F1FF}, {0x1F300, 0x1F5FF}, {0x1F700, 0x1F77F}, {0x1F680, 0x1F6FF}, {0x1F780, 0x1F7FF},
    {0x1F800, 0x1F8FF}, {0x1F650, 0x1F67F}, {0x1FA00, 0x1FA6F}, {0x1F0A0, 0x1F0FF}, {0x1F030, 0x1F09F},
    {0x1F000, 0x1F02F}, {0x1D300, 0x1D35F}, {0x10190, 0x101CF}, {0x101D0, 0x101FF}, {0x1FB00, 0x1FBFF},
    {0x1FA70, 0x1FAFF},
};

struct CharSet
{
    CharSet() = default;
    CharSet(const TKit::Span<const CodePointRange> ranges)
    {
        for (const CodePointRange &range : ranges)
            LoadRange(range);
    }
    CharSet(const std::initializer_list<CodePointRange> ranges) : CharSet({ranges.begin(), u32(ranges.size())})
    {
    }

    void LoadRange(const CodePointRange &range)
    {
        for (u32 c = range.First; c <= range.Last; ++c)
            CodePoints.Insert(c);
    }
    void Load(const u32 c)
    {
        CodePoints.Insert(c);
    }
    TKit::TierHashSet<u32> CodePoints{};
};

struct FontData
{
    TKit::TierArray<GlyphData> Glyphs{};
    TKit::TierHashMap<u32, u32> GlyphMap{}; // code point to idx into Glyphs
    TKit::TierHashMap<u64, f32> Kerning{};

    ImageData AtlasData{};
    f32 Ascender = 0.f;
    f32 Descender = 0.f;
    f32 LineHeight = 0.f;
    f32 UnitRange = 0.f;

    f32 GetKerning(const u32 code0, const u32 code1) const
    {
        const u64 key = u64(code0) << 32 | u64(code1);
        const auto it = Kerning.Find(key);
        if (it == Kerning.end())
            return 0.f;
        return it->Value;
    }
    const GlyphData *GetGlyph(const u32 code) const
    {
        const auto it = GlyphMap.Find(code);
        if (it == GlyphMap.end())
            return nullptr;
        return &Glyphs[it->Value];
    }

    f32v2 ComputeTextSize(TKit::StringView text) const;
    f32 ComputeTextWidth(TKit::StringView text) const
    {
        return ComputeTextSize(text)[0];
    }
    f32 GetLineFactor() const
    {
        return LineHeight / (Ascender - Descender);
    }
    f32 ComputeTextHeight(TKit::StringView text) const;

    f32 ComputeTextMinimumWidth(TKit::StringView text) const;
    TKit::String WrapText(TKit::StringView text, f32 maxWidth) const;
};

u32 DecodeUTF8(const char *code, u32 *count = nullptr);

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    CharSet CharSet{CharSets[CharSet_ASCII]};
    f32 Padding = 2.f;
    f32 SDFRange = 4.f;
    f32 FontScale = 1.f;
    f32 LineGapFactor = 1.2f;
    f32 MaxCornerAngle = 3.f;
    f32 EmSize = 40.f;
    u32 AtlasDimensions = 0; // zero means dimensions will be automatic
};
ONYX_NO_DISCARD Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts = {});
ONYX_NO_DISCARD Result<FontData> LoadFontDataFromMemory(const std::byte *memory, u32 size,
                                                        const FontLoadOptions &opts = {});
#    ifdef ONYX_INCLUDE_DEFAULT_FONT
ONYX_NO_DISCARD Result<FontData> LoadDefaultFont(const FontLoadOptions &opts = {});
#    endif
#endif
} // namespace Onyx
