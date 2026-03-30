#include "onyx/core/pch.hpp"
#include "onyx/asset/font.hpp"

#define MSDFGEN_PUBLIC
#include <msdfgen.h>
#include <msdfgen-ext.h>

namespace Onyx
{
Result<FontData> LoadFontDataFromFile(const char *path, const FontLoadOptions &opts)
{
    VKit::DeletionQueue dqueue{};
    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
        return Result<>::Error(Error_InitializationFailed, "[ONYX][FONT] Failed to initialize FreeType");

    dqueue.Push([ft] { msdfgen::deinitializeFreetype(ft); });

    msdfgen::FontHandle *font = msdfgen::loadFont(ft, path);
    if (!font)
        return Result<FontData>::Error(
            Error_Unknown,
            TKit::Format(
                "[ONYX][FONT] Failed to load font at {} because of an unknown reason (msdfgen does not expose why)",
                path));
    dqueue.Push([font] { msdfgen::destroyFont(font); });

    FontData data{};
    const TKit::Span<const CodePointRange> ranges = opts.CharSets;

    data.CharSetCount = ranges.GetSize();

    TextureData tdata{};
    tdata.Width = opts.AtlasWidth;
    tdata.Components = 4;
    tdata.Format = VK_FORMAT_R32G32B32A32_SFLOAT;

    u32 xatlas = 0;
    u32 yatlas = 0;
    u32 rowHeight = 0;

    const u32 w = opts.GlyphSize[0];
    const u32 h = opts.GlyphSize[1];

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
        {
            if (xatlas + w > opts.AtlasWidth)
            {
                xatlas = 0;
                yatlas += rowHeight;
                rowHeight = 0;
            }
            xatlas += w;
            rowHeight = Math::Max(rowHeight, h);
        }
    }
    tdata.Height = yatlas + rowHeight;

    const usz size = tdata.ComputeSize();
    tdata.Data = scast<std::byte *>(TKit::Allocate(size));
    dqueue.Push([tdata] { TKit::Deallocate(tdata.Data); });

    xatlas = 0;
    yatlas = 0;
    rowHeight = 0;

    const f32v2 scale = opts.GlyphSize - 2.f * opts.Padding;
    const f32v2 translate = opts.Padding / scale;
    const msdfgen::Projection proj{{scale[0], scale[1]}, {translate[0], translate[1]}};
    for (const CodePointRange &range : ranges)
    {
        for (u32 j = range.First; j <= range.Last; ++j)
        {
            msdfgen::Shape shape;
            f64 advance;
            if (!msdfgen::loadGlyph(shape, font, j, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance))
                return Result<FontData>::Error(
                    Error_Unknown,
                    "[ONYX][FONT] Failed to load glyph because of an unknown reason (msdfgen does not expose why)");

            shape.normalize();
            msdfgen::edgeColoringSimple(shape, 3.0);
            msdfgen::Bitmap<f32, 4> mtsdf{i32(w), i32(h)};
            msdfgen::generateMTSDF(mtsdf, shape, proj, opts.SDFRange);
            if (xatlas + w > opts.AtlasWidth)
            {
                xatlas = 0;
                yatlas += rowHeight;
                rowHeight = 0;
            }
            for (u32 k = 0; k < h; ++k)
            {
                const usz offset = ((yatlas + k) * tdata.Width + xatlas) * 4 * sizeof(f32);
                TKit::ForwardCopy(tdata.Data + offset, mtsdf(0, i32(k)), w * 4 * sizeof(f32));
            }

            GlyphData glyph{};
            glyph.MinTexCoord = f32v2{f32(xatlas) / tdata.Width, f32(yatlas) / tdata.Height};
            glyph.MaxTexCoord = f32v2{f32(xatlas + w) / tdata.Width, f32(yatlas + h) / tdata.Height};
            glyph.Advance = f32(advance);

            const msdfgen::Shape::Bounds bounds = shape.getBounds();
            glyph.Bearing = f32v2{f32(bounds.l), f32(bounds.t)};
            glyph.Size = f32v2{f32(bounds.r - bounds.l), f32(bounds.t - bounds.b)};

            data.Glyphs.Append(glyph);
            xatlas += w;
            rowHeight = Math::Max(rowHeight, h);
        }
        data.CodePoints.Append(range);
    }
    msdfgen::FontMetrics metrics;
    msdfgen::getFontMetrics(metrics, font);

    data.AtlasData = tdata;
    data.Ascender = f32(metrics.ascenderY);
    data.Descender = f32(metrics.descenderY);
    data.LineHeight = f32(metrics.lineHeight);

    return data;
}
} // namespace Onyx
