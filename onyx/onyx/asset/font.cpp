#include "onyx/core/pch.hpp"
#include "onyx/asset/font.hpp"

#ifdef ONYX_ENABLE_FONT_LOAD
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wint-in-bool-context")
TKIT_CLANG_WARNING_IGNORE("-Wunused-function")
TKIT_GCC_WARNING_IGNORE("-Wint-in-bool-context")
TKIT_GCC_WARNING_IGNORE("-Wunused-function")
#    define MSDFGEN_PUBLIC
#    include <msdf-atlas-gen/msdf-atlas-gen.h>
TKIT_COMPILER_WARNING_IGNORE_POP()
#endif

namespace Onyx
{
#ifdef ONYX_ENABLE_FONT_LOAD
Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts)
{
    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
        return Result<>::Error(Error_InitializationFailed, "[ONYX][FONT] Failed to initialize FreeType");

    msdfgen::FontHandle *font = msdfgen::loadFont(ft, path);
    if (!font)
    {
        msdfgen::deinitializeFreetype(ft);
        return Result<FontData>::Error(
            Error_Unknown,
            TKit::Format(
                "[ONYX][FONT] Failed to load font at {} because of an unknown reason (msdfgen does not expose why)",
                path));
    }
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

    msdf_atlas::TightAtlasPacker packer{};
    packer.setPixelRange(opts.SDFRange);
    packer.setMiterLimit(1.0);
    packer.setInnerPixelPadding(msdf_atlas::Padding{opts.Padding});
    packer.setScale(opts.EmSize);
    TKIT_CHECK_RETURNS(packer.pack(glyphs.data(), glyphs.size()), 0,
                       "[ONYX][FONT] Atlas packer did not pack all the glyphs!");

    i32 w;
    i32 h;
    packer.getDimensions(w, h);

    u64 seed = 0;
    for (msdf_atlas::GlyphGeometry &glyph : glyphs)
        glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, opts.MaxCornerAngle, seed++);

    msdf_atlas::ImmediateAtlasGenerator<f32, 4, msdf_atlas::mtsdfGenerator,
                                        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>
        generator{w, h};

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
    idata.Format = VK_FORMAT_R8G8B8A8_UNORM;

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

        gdata.Bounds.Min = quadMin;
        gdata.Bounds.Max = quadMax;
        gdata.Bounds.Center = 0.5f * (quadMin + quadMax);

        gdata.MinTexCoord = texCoordMin / idata.Width;
        gdata.MaxTexCoord = texCoordMax / idata.Height;

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

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);
    return data;
}
#endif

f32 FontData::GetKerning(const u32 code0, const u32 code1) const
{
    const u64 key = u64(code0) << 32 | u64(code1);
    auto it = std::lower_bound(Kerning.begin(), Kerning.end(), key,
                               [](const GlyphKerning &glyphs, const u64 k) { return glyphs.Key < k; });
    if (it != Kerning.end() && it->Key == key)
        return it->Kerning;
    return 0.0f;
}

} // namespace Onyx
