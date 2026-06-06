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
    CodePoint First;
    CodePoint Last;
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

struct CodePointRegistry
{
    CodePointRange ASCII;
    CodePointRange Latin1Supplement;
    CodePointRange LatinExtendedA;
    CodePointRange LatinExtendedB;
    CodePointRange Greek;
    CodePointRange Cyrillic;
    CodePointRange Arrows;
    CodePointRange MathOperators;
    CodePointRange MiscTechnical;
    CodePointRange EnclosedAlphanumerics;
    CodePointRange BoxDrawing;
    CodePointRange BlockElements;
    CodePointRange GeometricShapes;
    CodePointRange MiscSymbols;
    CodePointRange Dingbats;
    CodePointRange MiscSymbolsAndArrows;
    CodePointRange BraillePatterns;
    CodePointRange ControlPictures;
    CodePointRange OCR;
    CodePointRange YijingHexagrams;
    CodePointRange EnclosedAlphanumSupplement;
    CodePointRange MiscSymbolsAndPictographs;
    CodePointRange AlchemicalSymbols;
    CodePointRange TransportAndMap;
    CodePointRange GeometricShapesExtended;
    CodePointRange SupplementalArrowsC;
    CodePointRange OrnamentalDingbats;
    CodePointRange ChessSymbols;
    CodePointRange PlayingCards;
    CodePointRange DominoTiles;
    CodePointRange MahjongTiles;
    CodePointRange TaiXuanJing;
    CodePointRange AncientSymbols;
    CodePointRange PhaistosDisc;
    CodePointRange LegacyComputing;
    CodePointRange SymbolsPictographsExtA;
};

struct CharSetRegistry
{
    constexpr CharSetRegistry()
        : Named{
              .ASCII = {32, 126},
              .Latin1Supplement = {160, 255},
              .LatinExtendedA = {256, 383},
              .LatinExtendedB = {384, 591},
              .Greek = {880, 1023},
              .Cyrillic = {1024, 1279},
              .Arrows = {8592, 8703},
              .MathOperators = {8704, 8959},
              .MiscTechnical = {8960, 9215},
              .EnclosedAlphanumerics = {9312, 9471},
              .BoxDrawing = {9472, 9599},
              .BlockElements = {9600, 9631},
              .GeometricShapes = {9632, 9727},
              .MiscSymbols = {9728, 9983},
              .Dingbats = {9984, 10175},
              .MiscSymbolsAndArrows = {11008, 11263},
              .BraillePatterns = {10240, 10495},
              .ControlPictures = {9216, 9279},
              .OCR = {9280, 9311},
              .YijingHexagrams = {19904, 19967},
              .EnclosedAlphanumSupplement = {0x1F100, 0x1F1FF},
              .MiscSymbolsAndPictographs = {0x1F300, 0x1F5FF},
              .AlchemicalSymbols = {0x1F700, 0x1F77F},
              .TransportAndMap = {0x1F680, 0x1F6FF},
              .GeometricShapesExtended = {0x1F780, 0x1F7FF},
              .SupplementalArrowsC = {0x1F800, 0x1F8FF},
              .OrnamentalDingbats = {0x1F650, 0x1F67F},
              .ChessSymbols = {0x1FA00, 0x1FA6F},
              .PlayingCards = {0x1F0A0, 0x1F0FF},
              .DominoTiles = {0x1F030, 0x1F09F},
              .MahjongTiles = {0x1F000, 0x1F02F},
              .TaiXuanJing = {0x1D300, 0x1D35F},
              .AncientSymbols = {0x10190, 0x101CF},
              .PhaistosDisc = {0x101D0, 0x101FF},
              .LegacyComputing = {0x1FB00, 0x1FBFF},
              .SymbolsPictographsExtA = {0x1FA70, 0x1FAFF},
          }
    {
    }

    union {
        CodePointRange Flat[CharSet_Count];
        CodePointRegistry Named;
    };

    constexpr const CodePointRange &operator[](const u32 idx) const
    {
        return Flat[idx];
    }
};

constexpr CharSetRegistry CharSets{};

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
        for (CodePoint c = range.First; c <= range.Last; ++c)
            CodePoints.Insert(c);
    }
    void Load(const CodePoint c)
    {
        CodePoints.Insert(c);
    }
    TKit::TierHashSet<CodePoint> CodePoints{};
};

CodePoint DecodeUTF8(const char *code, u32 *count = nullptr);
u32 EncodeUTF8(char *buf, CodePoint code);

struct FontData
{
    TKit::TierArray<GlyphData> Glyphs{};
    TKit::TierHashMap<CodePoint, u32> GlyphMap{}; // code point to idx into Glyphs
    TKit::TierHashMap<u64, f32> Kerning{};

    ImageData AtlasData{};
    f32 Ascender = 0.f;
    f32 Descender = 0.f;
    f32 LineHeight = 0.f;
    f32 UnitRange = 0.f;

    template <typename F> void WalkText(const TKit::StringView text, F &&fun) const
    {
        u32 lastCode = TKIT_U32_MAX;
        for (u32 i = 0; i < text.GetSize();)
        {
            u32 byteCount;
            const CodePoint code = DecodeUTF8(&text[i], &byteCount);
            if (code == '\n')
            {
                if (!fun(i, byteCount, code, 0.f))
                    return;
                i += byteCount;
                continue;
            }
            const GlyphData *gdata = GetGlyph(code);
            if (!gdata)
            {
                TKIT_LOG_ERROR("[ONYX][FONT] The code U+{:04X} ({}) was not found as an available code point", code,
                               TKit::StringView{&text[i], byteCount});
                i += byteCount;
                continue;
            }
            const f32 width = gdata->Advance + GetKerning(lastCode, code);
            if (!fun(i, byteCount, code, width))
                return;

            i += byteCount;
            lastCode = code;
        }
    }

    f32 GetKerning(const CodePoint code0, const CodePoint code1) const
    {
        const u64 key = u64(code0) << 32 | u64(code1);
        const auto it = Kerning.Find(key);
        if (it == Kerning.end())
            return 0.f;
        return it->Value;
    }
    const GlyphData *GetGlyph(const CodePoint code) const
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

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    CharSet CharSet{CharSets[CharSet_ASCII]};
    f32 Padding = 2.f;
    f32 SDFRange = 4.f;
    f32 FontScale = 1.f;
    f32 LineGapFactor = 1.5f;
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
