#pragma once

#include "qol.slang"

struct Transform2D
{
    f32 Elm00, Elm01;
    f32 Elm10, Elm11;
    f32 Elm30, Elm31;
}

struct Transform3D
{
    f32 Elm00, Elm10, Elm20, Elm30;
    f32 Elm01, Elm11, Elm21, Elm31;
    f32 Elm02, Elm12, Elm22, Elm32;
}

struct WorldRect2D
{
    f32 Min[2];
    f32 Edge0[2];
    f32 Edge1[2];
}
struct WorldRect3D
{
    f32 Min[3];
    f32 Edge0[3];
    f32 Edge1[3];
    f32 Edge2[3];
}

struct InstanceData2D
{
    Transform2D Transform;
    WorldRect2D Rect;
    u32 FillColor;
    u32 OutlineColor;
    u32 MatOrSamplerTex;
    u32 TexOffset;
    u32 TexScale;
    f32 OutlineWidth;
    u32 DepthCounter;
}

struct InstanceData3D
{
    Transform3D Transform;
    WorldRect3D Rect;
    u32 FillColor;
    u32 OutlineColor;
    u32 MatOrSamplerTex;
    u32 TexOffset;
    u32 TexScale;
    f32 OutlineWidth;
}

struct BoundsData2D
{
    f32 Data[6];
}
struct BoundsData3D
{
    f32 Data[9];
}

struct SamplerTexId
{
    u32 Sampler;
    u32 Texture;

    bool IsNull()
    {
        return Sampler == ONYX_NULL_SAMPLER_ID || Texture == ONYX_NULL_TEXTURE_ID;
    }
}

[UnscopedEnum]
enum Alignment : u8
{
    Alignment_Left,
    Alignment_Center,
    Alignment_Right,
    Alignment_Bottom = Alignment_Left,
    Alignment_Top = Alignment_Right,
    Alignment_Near = Alignment_Left,
    Alignment_Far = Alignment_Right,
    Alignment_None = 3,
};

#define TKIT_F32_LOWEST -340282346638528859811704183484516925440.000000

bool CheckWorldClip(const f32v2 worldPos, const WorldRect2D rect)
{
    if (rect.Min[0] == TKIT_F32_LOWEST)
        return true;

    const f32v2 mn = f32v2(rect.Min[0], rect.Min[1]);
    const f32v2 edg0 = f32v2(rect.Edge0[0], rect.Edge0[1]);
    const f32v2 edg1 = f32v2(rect.Edge1[0], rect.Edge1[1]);

    const f32v2 dif = worldPos - mn;

    const f32 d0 = dot(dif, edg0);
    const f32 d1 = dot(dif, edg1);

    const f32 e0 = dot(edg0, edg0);
    const f32 e1 = dot(edg1, edg1);

    return d0 > 0.f && d0 < e0 && d1 > 0.f && d1 < e1;
}

bool CheckWorldClip(const f32v3 worldPos, const WorldRect3D rect)
{
    if (rect.Min[0] == TKIT_F32_LOWEST)
        return true;

    const f32v3 mn = f32v3(rect.Min[0], rect.Min[1], rect.Min[2]);
    const f32v3 edg0 = f32v3(rect.Edge0[0], rect.Edge0[1], rect.Edge0[2]);
    const f32v3 edg1 = f32v3(rect.Edge1[0], rect.Edge1[1], rect.Edge1[2]);
    const f32v3 edg2 = f32v3(rect.Edge2[0], rect.Edge2[1], rect.Edge2[2]);

    const f32v3 dif = worldPos - mn;

    const f32 d0 = dot(dif, edg0);
    const f32 d1 = dot(dif, edg1);
    const f32 d2 = dot(dif, edg2);

    const f32 e0 = dot(edg0, edg0);
    const f32 e1 = dot(edg1, edg1);
    const f32 e2 = dot(edg2, edg2);

    return d0 > 0.f && d0 < e0 && d1 > 0.f && d1 < e1 && d2 > 0.f && d2 < e2;
}

f32 ComputeDepth(const u32 depthCounter)
{
    return max(0.f, 1.f - f32(depthCounter) / f32(1U << 24));
}

f32m4 ConstructTransform(const Transform2D transform)
{
    return f32m4(
        f32v4(transform.Elm00, transform.Elm01, 0.f, 0.f),
        f32v4(transform.Elm10, transform.Elm11, 0.f, 0.f),
        f32v4(0.f,           0.f,           0.f, 0.f),
        f32v4(transform.Elm30, transform.Elm31, 0.f, 1.f)
    );
}

f32m4 ConstructTransform(const Transform3D transform)
{
    return f32m4(
        f32v4(transform.Elm00, transform.Elm01, transform.Elm02, 0.f),
        f32v4(transform.Elm10, transform.Elm11, transform.Elm12, 0.f),
        f32v4(transform.Elm20, transform.Elm21, transform.Elm22, 0.f),
        f32v4(transform.Elm30, transform.Elm31, transform.Elm32, 1.f)
    );
}

f32v2 ComputeStencilScale2D(const f32m4 transform)
{
    return f32v2(length(f32v2(transform[0][0], transform[1][0])), length(f32v2(transform[0][1], transform[1][1])));
}
f32v3 ComputeStencilScale3D(const f32m4 transform)
{
    return f32v3(length(f32v3(transform[0][0], transform[1][0], transform[2][0])), length(f32v3(transform[0][1], transform[1][1], transform[2][1])), length(f32v3(transform[0][2], transform[1][2], transform[2][2])));
}

f32m3 ComputeNormalMatrix(const f32m3 matrix)
{
    const f32v3 c0 = matrix[0];
    const f32v3 c1 = matrix[1];
    const f32v3 c2 = matrix[2];

    const f32v3 r0 = cross(c1, c2);
    const f32v3 r1 = cross(c2, c0);
    const f32v3 r2 = cross(c0, c1);

    const f32 det = dot(c0, r0);
    const f32 idet = sign(det) / max(abs(det), 1e-12);

    return f32m3(r0, r1, r2) * idet;
}

typedef StructuredBuffer<BoundsData2D, Std430DataLayout> Bounds2D;
typedef StructuredBuffer<BoundsData3D, Std430DataLayout> Bounds3D;
typedef StructuredBuffer<i32, Std430DataLayout> TextureOffsets;

f32v2 ComputeAlignment(const u32 alignment, const u32 id, const Bounds2D bounds)
{
    const BoundsData2D data = bounds[id];

    const u32 al0 = alignment & 0xFF;
    const u32 al1 = (alignment >> 8) & 0xFF;
    return f32v2(al0 == u32(Alignment_None) ? 0.f : data.Data[al0 * 2], al1 == u32(Alignment_None) ? 0.f : data.Data[al1 * 2 + 1]);
}

f32v3 ComputeAlignment(const u32 alignment, const u32 id, const Bounds3D bounds)
{
    const BoundsData3D data = bounds[id];

    const u32 al0 = alignment & 0xFF;
    const u32 al1 = (alignment >> 8) & 0xFF;
    const u32 al2 = (alignment >> 16) & 0xFF;
    return f32v3(al0 == u32(Alignment_None) ? 0.f : data.Data[al0 * 3], al1 == u32(Alignment_None) ? 0.f : data.Data[al1 * 3 + 1], al2 == u32(Alignment_None) ? 0.f : data.Data[al2 * 3 + 2]);
}


f32v2 ComputeTexCoord(const f32v2 uv0, const u32 offset, const u32 scale)
{
    const f32v2 uvOffset = unpackHalf2x16ToFloat(offset);
    const f32v2 uvScale = unpackHalf2x16ToFloat(scale);
    return uvOffset + uv0 * uvScale;
}

SamplerTexId GetSamplerTexId(const u32 id)
{
    const u32 sid = (id & ONYX_SAMPLER_ID_MASK) >> ONYX_SAMPLER_ID_SHIFT;
    const u32 tid = id & ONYX_TEXTURE_ID_MASK;
    return SamplerTexId(sid, tid);
}

SamplerTexId GetSamplerTexId(const TextureOffsets offsets, const u32 id)
{
    SamplerTexId combined = GetSamplerTexId(id);
    if (combined.IsNull())
        return combined;

    const u32 oid = (id & ONYX_TEXTURE_OFFSET_ID_MASK) >> ONYX_TEXTURE_OFFSET_ID_SHIFT;

    const i32 shifted = i32(combined.Texture) + offsets[oid];
    combined.Texture = u32(shifted);
    return combined;
}

f32v4 ComputeFlatColor(const SamplerState samplers[ONYX_MAX_SAMPLERS], const Texture2D textures[ONYX_MAX_TEXTURES], const TextureOffsets offsets, const u32 id, const f32v4 fill, const f32v2 uv0, const u32 offset, const u32 scale)
{
    const SamplerTexId stid = GetSamplerTexId(offsets, id);
    if (stid.IsNull())
        return fill;

    const f32v2 uv = ComputeTexCoord(uv0, offset, scale);
    const SamplerState samp = samplers[stid.Sampler];
    const Texture2D tex = textures[stid.Texture];

    return fill * tex.Sample(samp, uv);
}

