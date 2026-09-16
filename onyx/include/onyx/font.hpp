#pragma once

#include "onyx/math.hpp"
#include "onyx/image.hpp"
#include "tkit/container/hash_set.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/container/span.hpp"

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

struct UnicodeBlock
{
    CodePoint First;
    CodePoint Last;
};

enum Unicode : u32
{
    Unicode_ASCII,
    Unicode_Latin1Supplement,
    Unicode_LatinExtendedA,
    Unicode_LatinExtendedB,
    Unicode_Greek,
    Unicode_Cyrillic,
    Unicode_GeneralPunctuation,
    Unicode_Arrows,
    Unicode_MathOperators,
    Unicode_MiscTechnical,
    Unicode_EnclosedAlphanumerics,
    Unicode_BoxDrawing,
    Unicode_BlockElements,
    Unicode_GeometricShapes,
    Unicode_MiscSymbols,
    Unicode_Dingbats,
    Unicode_MiscSymbolsAndArrows,
    Unicode_BraillePatterns,
    Unicode_ControlPictures,
    Unicode_OCR,
    Unicode_YijingHexagrams,
    Unicode_EnclosedAlphanumSupplement,
    Unicode_MiscSymbolsAndPictographs,
    Unicode_AlchemicalSymbols,
    Unicode_TransportAndMap,
    Unicode_GeometricShapesExtended,
    Unicode_SupplementalArrowsC,
    Unicode_OrnamentalDingbats,
    Unicode_ChessSymbols,
    Unicode_PlayingCards,
    Unicode_DominoTiles,
    Unicode_MahjongTiles,
    Unicode_TaiXuanJing,
    Unicode_AncientSymbols,
    Unicode_PhaistosDisc,
    Unicode_LegacyComputing,
    Unicode_SymbolsPictographsExtA,
    Unicode_Count
};

struct UnicodeRegistry
{
    UnicodeBlock ASCII{32, 126};
    UnicodeBlock Latin1Supplement{160, 255};
    UnicodeBlock LatinExtendedA{256, 383};
    UnicodeBlock LatinExtendedB{384, 591};
    UnicodeBlock Greek{880, 1023};
    UnicodeBlock Cyrillic{1024, 1279};
    UnicodeBlock GeneralPunctuation{0x2000, 0x206F};
    UnicodeBlock Arrows{8592, 8703};
    UnicodeBlock MathOperators{8704, 8959};
    UnicodeBlock MiscTechnical{8960, 9215};
    UnicodeBlock EnclosedAlphanumerics{9312, 9471};
    UnicodeBlock BoxDrawing{9472, 9599};
    UnicodeBlock BlockElements{9600, 9631};
    UnicodeBlock GeometricShapes{9632, 9727};
    UnicodeBlock MiscSymbols{9728, 9983};
    UnicodeBlock Dingbats{9984, 10175};
    UnicodeBlock MiscSymbolsAndArrows{11008, 11263};
    UnicodeBlock BraillePatterns{10240, 10495};
    UnicodeBlock ControlPictures{9216, 9279};
    UnicodeBlock OCR{9280, 9311};
    UnicodeBlock YijingHexagrams{19904, 19967};
    UnicodeBlock EnclosedAlphanumSupplement{0x1F100, 0x1F1FF};
    UnicodeBlock MiscSymbolsAndPictographs{0x1F300, 0x1F5FF};
    UnicodeBlock AlchemicalSymbols{0x1F700, 0x1F77F};
    UnicodeBlock TransportAndMap{0x1F680, 0x1F6FF};
    UnicodeBlock GeometricShapesExtended{0x1F780, 0x1F7FF};
    UnicodeBlock SupplementalArrowsC{0x1F800, 0x1F8FF};
    UnicodeBlock OrnamentalDingbats{0x1F650, 0x1F67F};
    UnicodeBlock ChessSymbols{0x1FA00, 0x1FA6F};
    UnicodeBlock PlayingCards{0x1F0A0, 0x1F0FF};
    UnicodeBlock DominoTiles{0x1F030, 0x1F09F};
    UnicodeBlock MahjongTiles{0x1F000, 0x1F02F};
    UnicodeBlock TaiXuanJing{0x1D300, 0x1D35F};
    UnicodeBlock AncientSymbols{0x10190, 0x101CF};
    UnicodeBlock PhaistosDisc{0x101D0, 0x101FF};
    UnicodeBlock LegacyComputing{0x1FB00, 0x1FBFF};
    UnicodeBlock SymbolsPictographsExtA{0x1FA70, 0x1FAFF};
};

constexpr UnicodeRegistry UnicodeBlocks{};
constexpr const UnicodeBlock *UnicodeBlocksArray = &UnicodeBlocks.ASCII;

struct CharSet
{
    CharSet() = default;
    CharSet(const TKit::Span<const UnicodeBlock> blocks)
    {
        for (const UnicodeBlock &range : blocks)
            LoadBlock(range);
    }
    CharSet(const std::initializer_list<UnicodeBlock> blocks) : CharSet({blocks.begin(), u32(blocks.size())})
    {
    }

    void LoadBlock(const UnicodeBlock &range)
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
            const GlyphData *gdata = GetGlyphData(code);
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
    const GlyphData *GetGlyphData(const CodePoint code) const
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
    TKit::TierString WrapText(TKit::StringView text, f32 maxWidth) const;
};

#ifdef ONYX_ENABLE_FONT_LOAD
struct FontLoadOptions
{
    CharSet CharSet{UnicodeBlocks.ASCII};
    f32 Padding = 2.f;
    f32 SDFRange = 4.f;
    f32 FontScale = 1.f;
    f32 LineGapFactor = 1.5f;
    f32 MaxCornerAngle = 3.f;
    f32 EmSize = 40.f;
    u32 AtlasDimensions = 0; // zero means dimensions will be automatic
};
ONYX_NO_DISCARD Result<FontData> Font_LoadDataFromFile(const char *path, const FontLoadOptions &opts = {});
ONYX_NO_DISCARD Result<FontData> Font_LoadDataFromMemory(const std::byte *memory, u32 size,
                                                         const FontLoadOptions &opts = {});
#    ifdef ONYX_INCLUDE_DEFAULT_FONT
ONYX_NO_DISCARD Result<FontData> Font_LoadDefaultData(const FontLoadOptions &opts = {});
#    endif

void Font_UnloadData(const FontData &data);
#endif
} // namespace Onyx
