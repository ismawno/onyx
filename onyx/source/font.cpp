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
#        include "ttf.hpp"
#    endif
#endif

namespace Onyx
{
#ifdef ONYX_ENABLE_FONT_LOAD
ONYX_NO_DISCARD static Result<FontData> loadFont(msdfgen::FreetypeHandle *ft, msdfgen::FontHandle *font,
                                                 const FontLoadOptions &opts)
{
    std::vector<msdf_atlas::GlyphGeometry> glyphs;
    msdf_atlas::FontGeometry fgeo{&glyphs};

    FontData data{};
    TKIT_ASSERT(!opts.CharSet.CodePoints.IsEmpty(), "[ONYX][FONT] The char set must not be empty");

    msdf_atlas::Charset chset{};
    for (const u32 c : opts.CharSet.CodePoints)
        chset.add(c);

    const u32 loaded = u32(fgeo.loadCharset(font, opts.FontScale, chset));
    TKIT_LOG_DEBUG("[ONYX][FONT] Loaded {}/{} glyphs", loaded, chset.size());
    if (loaded == 0)
        return Result<FontData>::Error(Error_LoadFailed, "[ONYX][FONT] No glyph was able to be loaded");

    msdf_atlas::TightAtlasPacker packer{};
    if (opts.AtlasDimensions != 0)
    {
        const i32 dim = i32(opts.AtlasDimensions);
        packer.setDimensions(dim, dim);
    }
    else
        packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::SQUARE);

    packer.setPixelRange(opts.SDFRange);
    packer.setMiterLimit(1.0);
    packer.setInnerPixelPadding(msdf_atlas::Padding{opts.Padding});
    packer.setScale(opts.EmSize);

    const i32 left = packer.pack(glyphs.data(), i32(glyphs.size()));
    if (left != 0)
        return Result<FontData>::Error(
            Error_LoadFailed,
            TKit::Format("[ONYX][FONT] The atlas packer was not able to pack all the glyphs. Remaining: {}", left));

    i32v2 dims;
    packer.getDimensions(dims[0], dims[1]);
    TKIT_LOG_DEBUG("[ONYX][FONT] Atlas dimensions are {}x{}", dims[0], dims[1]);
    TKIT_ASSERT(dims[0] == dims[1], "[ONYX][FONT] Atlas dimensions must be equal, but w = {} != {} = h", dims[0],
                dims[1]);

    u64 seed = 0;
    for (msdf_atlas::GlyphGeometry &glyph : glyphs)
        glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, opts.MaxCornerAngle, seed++);

    msdf_atlas::ImmediateAtlasGenerator<f32, 4, msdf_atlas::mtsdfGenerator,
                                        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>
        generator{dims[0], dims[1]};

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

    TKit::StackArray<u8> flipped{};
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

    msdfgen::FontMetrics metrics;
    msdfgen::getFontMetrics(metrics, font);

    data.AtlasData = idata;
    data.Ascender = f32(metrics.ascenderY) / opts.EmSize;
    data.Descender = f32(metrics.descenderY) / opts.EmSize;
    data.LineHeight = opts.LineGapFactor * f32(metrics.lineHeight) / opts.EmSize;
    data.UnitRange = opts.SDFRange / f32(dims[0]);

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

        data.GlyphMap[glyph.getCodepoint()] = data.Glyphs.GetSize();
        data.Glyphs.Append(gdata);
    }
    for (const msdf_atlas::GlyphGeometry &g1 : glyphs)
        for (const msdf_atlas::GlyphGeometry &g2 : glyphs)
        {
            const msdf_atlas::unicode_t a = g1.getCodepoint();
            const msdf_atlas::unicode_t b = g2.getCodepoint();

            const u64 key = u64(a) << 32 | u64(b);
            f64 kerning = 0.;
            msdfgen::getKerning(kerning, font, a, b, msdfgen::FONT_SCALING_EM_NORMALIZED);
            if (kerning != 0.)
                data.Kerning[key] = f32(kerning);
        }

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
        msdfgen::loadFontData(ft, NotoSansSymbols_Merged_ttf, i32(NotoSansSymbols_Merged_ttf_len));
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

// NOTE(Isma): At some point we may need to cache/use explicit utf8 strings if this decoding thing becomes a problem
u32 DecodeUTF8(const char *code, u32 *count)
{
    if (count)
        *count = 1;
    const u8 byte = *code++;
    if (byte < 0x80)
        return byte;

    u32 cp;
    u32 remaining;

    if ((byte & 0xE0) == 0xC0)
    {
        cp = byte & 0x1F;
        remaining = 1;
    }
    else if ((byte & 0xF0) == 0xE0)
    {
        cp = byte & 0x0F;
        remaining = 2;
    }
    else if ((byte & 0xF8) == 0xF0)
    {
        cp = byte & 0x07;
        remaining = 3;
    }
    else
        return 0xFFFD;

    for (u32 i = 0; i < remaining; ++i)
    {
        if ((*code & 0xC0) != 0x80)
            return 0xFFFD;

        if (count)
            ++(*count);
        cp = (cp << 6) | (*code++ & 0x3F);
    }
    return cp;
}

f32v2 FontData::ComputeTextSize(const TKit::StringView text) const
{
    const f32 lheight = LineHeight;
    u32 nlcount = 1;
    f32 width = 0.f;
    f32 lastWidth = 0.f;

    u32 lastCode = TKIT_U32_MAX;
    for (u32 i = 0; i < text.GetSize();)
    {
        u32 byteCount;
        const u32 code = DecodeUTF8(&text[i], &byteCount);

        if (code == '\n')
        {
            ++nlcount;
            lastWidth = Math::Max(width, lastWidth);
            width = 0.f;
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

        if (i != 0)
            width += GetKerning(lastCode, code);

        width += gdata->Advance;
        i += byteCount;
        lastCode = code;
    }
    return f32v2{Math::Max(width, lastWidth), nlcount * lheight};
}

f32 FontData::ComputeTextHeight(const TKit::StringView text) const
{
    const f32 lheight = LineHeight;
    u32 nlcount = 1;
    for (const char c : text)
        // NOTE(Isma): No decode here. we sould be fine
        if (c == '\n')
            ++nlcount;
    return nlcount * lheight;
}

f32 FontData::ComputeTextMinimumWidth(const TKit::StringView text) const
{
    if (text.IsEmpty())
        return 0.f;

    f32 size = 0.f;
    u32 start = 0;
    u32 end = 0;
    for (u32 i = 0; i < text.GetSize(); ++i)
    {
        const char c = text[i];
        if (c == '\n' || c == ' ')
        {
            end = i;
            size = Math::Max(size, ComputeTextWidth(text.SubString(start, end)));
            start = end;
        }
    }
    if (start < end)
        size = Math::Max(size, ComputeTextWidth(text.SubString(start, end)));
    else if (end == 0)
        size = ComputeTextWidth(text);
    return size;
}

TKit::String FontData::WrapText(const TKit::StringView text, const f32 maxWidth) const
{
    TKit::String wrapped;
    wrapped.Reserve(text.GetSize());

    f32 size = 0.f;
    f32 lastSize = 0.f;
    u32 lastSpace = TKIT_U32_MAX;

    u32 lastCode = TKIT_U32_MAX;
    for (u32 i = 0; i < text.GetSize();)
    {
        u32 byteCount;
        const u32 code = DecodeUTF8(&text[i], &byteCount);

        for (u32 j = 0; j < byteCount; ++j)
            wrapped.Append(text[i + j]);

        if (code == '\n')
        {
            size = 0.f;
            i += byteCount;
            continue;
        }
        if (code == ' ')
        {
            lastSpace = i;
            lastSize = size;
        }

        const GlyphData *gdata = GetGlyph(code);
        if (!gdata)
        {
            TKIT_LOG_ERROR("[ONYX][FONT] The code U+{:04X} ({}) was not found as an available code point", code,
                           TKit::StringView{&text[i], byteCount});
            i += byteCount;
            continue;
        }

        if (i != 0)
            size += GetKerning(lastCode, code);

        size += gdata->Advance;
        if (size > maxWidth && lastSpace != TKIT_U32_MAX)
        {
            wrapped[lastSpace] = '\n';
            lastSpace = TKIT_U32_MAX;
            size -= lastSize;
        }

        i += byteCount;
        lastCode = code;
    }
    return wrapped;
}
} // namespace Onyx
