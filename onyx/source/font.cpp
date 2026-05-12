#include "pch.hpp"
#include "onyx/font.hpp"

#ifdef ONYX_ENABLE_FONT_LOAD
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wint-in-bool-context")
TKIT_CLANG_WARNING_IGNORE("-Wunused-function")
TKIT_GCC_WARNING_IGNORE("-Wint-in-bool-context")
TKIT_GCC_WARNING_IGNORE("-Wunused-function")
TKIT_MSVC_WARNING_IGNORE(4458)
TKIT_MSVC_WARNING_IGNORE(4505)
#    define MSDFGEN_PUBLIC
#    include <msdf-atlas-gen/msdf-atlas-gen.h>
TKIT_COMPILER_WARNING_IGNORE_POP()
#    ifdef ONYX_INCLUDE_DEFAULT_FONT
#        include "font.hpp"
#    endif
#endif

namespace Onyx
{
#ifdef ONYX_ENABLE_FONT_LOAD
ONYX_NO_DISCARD static Result<FontData> loadFont(msdfgen::FreetypeHandle *ft, msdfgen::FontHandle *font,
                                                 const FontLoadOptions &opts)
{
    const TKit::Span<const CodePointRange> ranges = opts.CharSets;

    std::vector<msdf_atlas::GlyphGeometry> glyphs;
    msdf_atlas::FontGeometry fgeo{&glyphs};
    msdf_atlas::Charset chset{};

    FontData data{};
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        const CodePointRange &range = ranges[i];
        TKIT_ASSERT(range.First < range.Last,
                    "[ONYX][FONT] Code point range must not be empty or contain a negative range, but found First = "
                    "{} and Last = {}",
                    range.First, range.Last);
        TKIT_ASSERT(i == 0 || (range.First > ranges[i - 1].First && range.Last > ranges[i - 1].Last),
                    "[ONYX][FONT] Code point ranges must not interleave and must be sorted, but found range {} with "
                    "First = {}, Last = {} and range {} with First = {}, Last = {}",
                    i, range.First, range.Last, i - 1, ranges[i - 1].First, ranges[i - 1].Last);
        for (u32 j = range.First; j <= range.Last; ++j)
            chset.add(j);
        data.CodePoints.Append(range);
    }
    const u32 count = u32(fgeo.loadCharset(font, opts.FontScale, chset));
    TKIT_LOG_DEBUG("[ONYX][FONT] Loaded {}/{} glyphs", count, chset.size());
    TKIT_UNUSED(count);

    const i32 dim = i32(opts.AtlasDimensions);
    msdf_atlas::TightAtlasPacker packer{};
    packer.setDimensions(dim, dim);
    packer.setPixelRange(opts.SDFRange);
    packer.setMiterLimit(1.0);
    packer.setInnerPixelPadding(msdf_atlas::Padding{opts.Padding});
    packer.setScale(opts.EmSize);
    TKIT_CHECK_RETURNS(packer.pack(glyphs.data(), i32(glyphs.size())), 0,
                       "[ONYX][FONT] Atlas packer did not pack all the glyphs!");

    u64 seed = 0;
    for (msdf_atlas::GlyphGeometry &glyph : glyphs)
        glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, opts.MaxCornerAngle, seed++);

    msdf_atlas::ImmediateAtlasGenerator<f32, 4, msdf_atlas::mtsdfGenerator,
                                        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>
        generator{dim, dim};

    msdf_atlas::GeneratorAttributes att;
    att.config.overlapSupport = true;
    att.scanlinePass = true;
    generator.setAttributes(att);
    generator.setThreadCount(TKIT_MAX_THREADS);
    generator.generate(glyphs.data(), i32(glyphs.size()));

    const msdfgen::BitmapConstRef<u8, 4> bitmap = generator.atlasStorage();
    ImageData idata;
    idata.Width = u32(bitmap.width);
    idata.Height = u32(bitmap.height);
    idata.Format = Format_R8G8B8A8_UNORM;

    TKit::TierArray<u8> flipped{};
    flipped.Reserve(idata.Width * idata.Height * 4);

    for (u32 i = 0; i < idata.Height; ++i)
    {
        const u8 *src = bitmap(0, idata.Height - i - 1);
        u8 *dst = flipped.GetData() + i * idata.Width * 4;
        TKit::ForwardCopy(dst, src, idata.Width * 4);
    }

    const usz size = idata.Width * idata.Height * 4;
    idata.Data = scast<std::byte *>(TKit::Allocate(size));
    TKit::ForwardCopy(idata.Data, flipped.GetData(), size);

    data.AtlasData = idata;

    for (const msdf_atlas::GlyphGeometry &glyph : glyphs)
    {
        GlyphData gdata{};
        gdata.Advance = f32(glyph.getAdvance());

        f64 pl, pb, pr, pt;
        glyph.getQuadPlaneBounds(pl, pb, pr, pt);
        const f32v2 quadMin{f32(pl), f32(pb)};
        const f32v2 quadMax{f32(pr), f32(pt)};

        f64 al, ab, ar, at;
        glyph.getQuadAtlasBounds(al, ab, ar, at);
        const f32v2 texCoordMin{f32(al), idata.Height - f32(ab)};
        const f32v2 texCoordMax{f32(ar), idata.Height - f32(at)};

        gdata.Min = quadMin;
        gdata.Max = quadMax;

        const f32v2 atlasSize{f32(idata.Width), f32(idata.Height)};
        gdata.MinTexCoord = texCoordMin / atlasSize;
        gdata.MaxTexCoord = texCoordMax / atlasSize;

        data.Glyphs.Append(gdata);
    }
    for (msdf_atlas::unicode_t a : chset)
        for (msdf_atlas::unicode_t b : chset)
        {
            const u64 key = u64(a) << 32 | u64(b);
            f64 kerning = 0.;
            msdfgen::getKerning(kerning, font, a, b, msdfgen::FONT_SCALING_EM_NORMALIZED);
            if (kerning != 0.)
                data.Kerning.Append(key, f32(kerning));
        }
    std::sort(data.Kerning.begin(), data.Kerning.end(),
              [](const GlyphKerning &a, const GlyphKerning &b) { return a.Key < b.Key; });

    msdfgen::FontMetrics metrics;
    msdfgen::getFontMetrics(metrics, font);

    data.AtlasData = idata;
    data.Ascender = f32(metrics.ascenderY);
    data.Descender = f32(metrics.descenderY);
    data.LineHeight = f32(metrics.lineHeight);
    data.UnitRange = opts.SDFRange / f32(dim);

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);
    return data;
}
Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts)
{
    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
        return Result<>::Error(Error_LoadFailed, "[ONYX][FONT] Failed to initialize FreeType");

    msdfgen::FontHandle *font = msdfgen::loadFont(ft, path);
    if (!font)
    {
        msdfgen::deinitializeFreetype(ft);
        return Result<FontData>::Error(
            Error_Unknown,
            TKit::String::Format("[ONYX][FONT] Failed to load font at {} because of an unknown reason", path));
    }
    return loadFont(ft, font, opts);
}

Result<FontData> LoadFontDataFromMemory(const std::byte *memory, const u32 size, const FontLoadOptions &opts)
{
    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
        return Result<>::Error(Error_LoadFailed, "[ONYX][FONT] Failed to initialize FreeType");

    msdfgen::FontHandle *font = msdfgen::loadFontData(ft, rcast<const unsigned char *>(memory), i32(size));
    if (!font)
    {
        msdfgen::deinitializeFreetype(ft);
        return Result<FontData>::Error(Error_Unknown,
                                       "[ONYX][FONT] Failed to load font at {} because of an unknown reason");
    }
    return loadFont(ft, font, opts);
}
#    ifdef ONYX_INCLUDE_DEFAULT_FONT
Result<FontData> LoadDefaultFont(const FontLoadOptions &opts)
{
    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
        return Result<>::Error(Error_LoadFailed, "[ONYX][FONT] Failed to initialize FreeType");

    msdfgen::FontHandle *font =
        msdfgen::loadFontData(ft, Inter_VariableFont_opsz_wght_ttf, i32(Inter_VariableFont_opsz_wght_ttf_len));
    if (!font)
    {
        msdfgen::deinitializeFreetype(ft);
        return Result<FontData>::Error(Error_Unknown,
                                       "[ONYX][FONT] Failed to load font at {} because of an unknown reason");
    }
    return loadFont(ft, font, opts);
}
#    endif
#endif

u32 FontData::GetGlyphDataIndex(const u32 code) const
{
    u32 csize = 0;
    for (const CodePointRange &range : CodePoints)
    {
        if (code >= range.First && code <= range.Last)
            return code - range.First + csize;
        csize += range.Last - range.First + 1;
    }
    return TKIT_U32_MAX;
}

f32 FontData::GetKerning(const u32 code0, const u32 code1) const
{
    const u64 key = u64(code0) << 32 | u64(code1);
    auto it = std::lower_bound(Kerning.begin(), Kerning.end(), key,
                               [](const GlyphKerning &glyphs, const u64 k) { return glyphs.Key < k; });
    if (it != Kerning.end() && it->Key == key)
        return it->Kerning;
    return 0.0f;
}

f32 FontData::ComputeTextSize(const TKit::StringView text) const
{
    f32 size = 0.f;
    for (u32 i = 0; i < text.GetSize(); ++i)
    {
        const char c = text[i];
        if (c == '\n')
            continue;

        const u32 idx = GetGlyphDataIndex(c);
        if (idx == TKIT_U32_MAX)
        {
            TKIT_LOG_ERROR("[ONYX][FONT] The character {} was not found as an available code point", c);
            continue;
        }

        if (i != 0)
            size += GetKerning(text[i - 1], c);

        size += Glyphs[idx].Advance;
    }
    return size;
}

f32 FontData::ComputeTextMinimumSize(const TKit::StringView text) const
{
    f32 size = 0.f;
    u32 start = 0;
    u32 end = 0;
    for (u32 i = 0; i < text.GetSize();)
    {
        const char c = text[i];
        if (c == '\n' || c == ' ')
        {
            end = i;
            size = Math::Max(size, ComputeTextSize(text.SubString(start, end)));
            start = end;
        }
    }
    if (start < end)
        size = Math::Max(size, ComputeTextSize(text.SubString(start, end)));
    return size;
}

TKit::String FontData::WrapText(const TKit::StringView text, const f32 maxWidth) const
{
    TKit::String wrapped;
    wrapped.Reserve(text.GetSize());

    u32 lastSpace = TKIT_U32_MAX;
    f32 size = 0.f;

    for (u32 i = 0; i < text.GetSize(); ++i)
    {
        const char c = text[i];
        wrapped.Append(c);

        if (c == '\n')
        {
            size = 0.f;
            continue;
        }
        if (c == ' ')
            lastSpace = i;

        const u32 idx = GetGlyphDataIndex(c);
        if (idx == TKIT_U32_MAX)
        {
            TKIT_LOG_ERROR("[ONYX][FONT] The character {} was not found as an available code point", c);
            continue;
        }

        if (i != 0)
            size += GetKerning(text[i - 1], c);

        size += Glyphs[idx].Advance;
        if (size >= maxWidth && lastSpace != TKIT_U32_MAX)
        {
            wrapped[lastSpace] = '\n';
            lastSpace = TKIT_U32_MAX;
            size = 0.f;
        }
    }
    return wrapped;
}
} // namespace Onyx
