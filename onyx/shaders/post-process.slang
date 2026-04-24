#include "qol.slang"

static const f32v2 g_Positions[6] =
{
    f32v2(-1.0, -1.0),
    f32v2( 1.0, -1.0),
    f32v2(-1.0,  1.0),
    f32v2( 1.0, -1.0),
    f32v2(-1.0,  1.0),
    f32v2( 1.0,  1.0),
};

struct FragInput
{
    f32v4 Position : SV_Position;
};

[shader("vertex")]
FragInput mainVS(const u32 vertexId : SV_VertexID)
{
    FragInput finput;
    finput.Position = f32v4(g_Positions[vertexId], 0.f, 1.f);
    return finput;
}

[UnscopedEnum]
enum PostProcessFlagBit : u32
{
    PostProcessFlag_Outlines = 1 << 2,
};

struct PushConstants
{
    u32v2 Extent;
    u32 AttachmentIndex;
    u32 MaxOutlineWidth;
    u32 Flags;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_POST_PROCESS_COLOR_ATTACHMENTS_BINDING)]]
Texture2D g_Colors[ONYX_MAX_ATTACHMENTS];

[[vk::binding(ONYX_POST_PROCESS_OUTLINE_ATTACHMENTS_BINDING)]]
Texture2D g_Outlines[ONYX_MAX_ATTACHMENTS];

[[vk::binding(ONYX_POST_PROCESS_STENCIL_ATTACHMENTS_BINDING)]]
Texture2D<u32> g_Stencil[ONYX_MAX_ATTACHMENTS];

//TODO(Isma): Use samplers
i32v2 ClampPixels(const i32v2 pixels)
{
    return clamp(pixels, i32v2(0), i32v2(g_PushData.Extent) - i32v2(1));
}

f32v4 LoadTex(const Texture2D texs[ONYX_MAX_ATTACHMENTS], const i32v2 coords)
{
    const u32 idx = g_PushData.AttachmentIndex;
    const Texture2D tex = texs[idx];
    return tex.Load(i32v3(ClampPixels(coords), 0));
}

f32v4 LoadColor(const i32v2 coords)
{
    return LoadTex(g_Colors, coords);
}

f32v4 LoadOutline(const i32v2 coords)
{
    return LoadTex(g_Outlines, coords);
}

u32 LoadStencil(const i32v2 coords)
{
    const u32 idx = g_PushData.AttachmentIndex;
    const Texture2D<u32> stex = g_Stencil[idx];
    return stex.Load(i32v3(ClampPixels(coords), 0));
}

[shader("fragment")]
f32v4 mainFS(const FragInput input) : SV_Target
{
    const i32v2 center = i32v2(input.Position.xy);
    if (!bool(g_PushData.Flags & PostProcessFlag_Outlines) || LoadStencil(center) != 0)
        return LoadColor(center);

    const i32 maxWidth = i32(g_PushData.MaxOutlineWidth);
    const i32 dmaxWidth = i32(maxWidth * 0.707);

    const i32v2 maxOffsets[8] = {i32v2(-maxWidth, 0), i32v2(maxWidth, 0), i32v2(0, -maxWidth), i32v2(0, maxWidth), i32v2(-dmaxWidth, -dmaxWidth), i32v2(dmaxWidth, -dmaxWidth), i32v2(-dmaxWidth, dmaxWidth), i32v2(dmaxWidth, dmaxWidth)};

    f32v3 outline;
    u32 width = 0;
    for (u32 i = 0; i < 8; ++i)
    {
        const i32v2 offset = maxOffsets[i];
        const i32v2 coord = center + offset;
        const u32 stencil = LoadStencil(coord);
        if (stencil != 0)
        {
            const f32v4 outl = LoadOutline(coord);
            width = u32(outl.a * f32(maxWidth));
            outline = outl.rgb;
            break;
        }
    }

    if (width == 0)
        return LoadColor(center);

    const i32 dwidth = i32(width * 0.707);
    const i32v2 offsets[8] = {i32v2(-width, 0), i32v2(width, 0), i32v2(0, -width), i32v2(0, width), i32v2(-dwidth, -dwidth), i32v2(dwidth, -dwidth), i32v2(-dwidth, dwidth), i32v2(dwidth, dwidth)};

    for (u32 i = 0; i < 8; ++i)
    {
        const i32v2 offset = offsets[i];
        const i32v2 coord = center + offset;
        const u32 stencil = LoadStencil(coord);
        if (stencil != 0)
            return f32v4(outline, 1.f);
    }
    return LoadColor(center);
}

