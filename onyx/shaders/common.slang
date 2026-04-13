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

struct InstanceData2D
{
    Transform2D Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
    f32 OutlineWidth;
    u32 DepthCounter;
}

struct InstanceData3D
{
    Transform3D Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
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

u32 GetAssetId(const u32 handle)
{
    return handle & ONYX_ASSET_ID_MASK;
}

f32 ComputeDepth(const u32 depthCounter)
{
    return max(0.f, 1.f - f32(depthCounter) / f32(1U << 24));
}

f32m4 ComputeTransform2D(const Transform2D transform)
{
    return f32m4(
        f32v4(transform.Elm00, transform.Elm01, 0.f, 0.f),
        f32v4(transform.Elm10, transform.Elm11, 0.f, 0.f),
        f32v4(0.f,           0.f,           0.f, 0.f),
        f32v4(transform.Elm30, transform.Elm31, 0.f, 1.f)
    );
}

f32m4 ComputeTransform3D(const Transform3D transform)
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

f32v2 GetCenter2D(const u32 handle, const Bounds2D bounds)
{
    const BoundsData2D data = bounds[GetAssetId(handle)];
    return f32v2(data.Data[2], data.Data[3]);
}

f32v2 ComputeAlignment2D(const u32 alignment, const u32 handle, const Bounds2D bounds)
{
    const BoundsData2D data = bounds[GetAssetId(handle)];

    const u32 al0 = alignment & 0xFF;
    const u32 al1 = (alignment >> 8) & 0xFF;
    return f32v2(al0 == u32(Alignment_None) ? 0.f : data.Data[al0 * 2], al1 == u32(Alignment_None) ? 0.f : data.Data[al1 * 2 + 1]);
}

f32v3 GetCenter3D(const u32 handle, const Bounds3D bounds)
{
    const BoundsData3D data = bounds[GetAssetId(handle)];
    return f32v3(data.Data[3], data.Data[4], data.Data[5]);
}

f32v3 ComputeAlignment3D(const u32 alignment, const u32 handle, const Bounds3D bounds)
{
    const BoundsData3D data = bounds[GetAssetId(handle)];

    const u32 al0 = alignment & 0xFF;
    const u32 al1 = (alignment >> 8) & 0xFF;
    const u32 al2 = (alignment >> 16) & 0xFF;
    return f32v3(al0 == u32(Alignment_None) ? 0.f : data.Data[al0 * 3], al1 == u32(Alignment_None) ? 0.f : data.Data[al1 * 3 + 1], al2 == u32(Alignment_None) ? 0.f : data.Data[al2 * 3 + 2]);
}

