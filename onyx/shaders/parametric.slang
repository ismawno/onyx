#pragma once

#include "common.slang"

typedef StructuredBuffer<ParametricInstanceData2D, Std430DataLayout> ParametricInstances2D;
typedef StructuredBuffer<ParametricInstanceData3D, Std430DataLayout> ParametricInstances3D;

[UnscopedEnum]
enum ParametricShape : u32
{
    ParametricShape_Stadium,
    ParametricShape_RoundedQuad,
    ParametricShape_Capsule,
    ParametricShape_RoundedBox,
    ParametricShape_Torus,
    ParametricShape_Ring,
};

typedef u32 ParametricRegionFlags;

[UnscopedEnum]
enum StadiumRegion : ParametricRegionFlags
{
    StadiumRegion_Edge = 1 << 0,
    StadiumRegion_Moon = 1 << 1,
};

[UnscopedEnum]
enum RoundedQuadRegion : ParametricRegionFlags
{
    RoundedQuadRegion_HorizontalEdge = 1 << 0,
    RoundedQuadRegion_VerticalEdge = 1 << 1,
    RoundedQuadRegion_Moon = 1 << 2,
};

[UnscopedEnum]
enum CapsuleRegion : ParametricRegionFlags
{
    CapsuleRegion_Body = 1 << 0,
};

struct InstanceParameters
{
    f32 Payload[4];
    ParametricShape Shape;
}

struct ParametricInstanceData2D
{
    InstanceData2D Data;
    u32 BoundsHandle;
    InstanceParameters Parameters;
}
struct ParametricInstanceData3D
{
    InstanceData3D Data;
    u32 BoundsHandle;
    InstanceParameters Parameters;
}

// TODO(Isma): Take into account alignment!
f32v2 GetParametricPosition2D(f32v2 pos, const InstanceParameters params, const ParametricRegionFlags region)
{
    if (params.Shape == ParametricShape_Stadium)
    {
        const f32v2 dims = f32v2(2.f * params.Payload[1], params.Payload[0]);
        const f32v2 s = sign(pos);
        pos += 0.5f * (dims - 1.f) * s;
        if (bool(region & StadiumRegion_Edge))
            pos[1] += (params.Payload[1] - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_RoundedQuad)
    {
        const f32v2 dims = f32v2(params.Payload[0], params.Payload[1]);
        const f32 radius = params.Payload[2];
        const f32v2 s = sign(pos);
        pos += 0.5f * (dims - 1.f) * s;
        if (bool(region & RoundedQuadRegion_HorizontalEdge))
            pos[0] += (radius - 0.5f) * s[0];
        if (bool(region & RoundedQuadRegion_VerticalEdge))
            pos[1] += (radius - 0.5f) * s[1];
    }
    return pos;
}

// TODO(Isma): Take into account alignment!
f32v3 GetParametricPosition3D(f32v3 pos, const f32v3 normal, const InstanceParameters params, const ParametricRegionFlags region)
{
    if (params.Shape == ParametricShape_Stadium)
    {
        const f32v2 dims = f32v2(2.f * params.Payload[1], params.Payload[0]);
        const f32v2 s = sign(pos.xy);
        pos.xy += 0.5f * (dims - 1.f) * s;
        if (bool(region & StadiumRegion_Edge))
            pos[1] += (params.Payload[1] - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_RoundedQuad)
    {
        const f32v2 dims = f32v2(params.Payload[0], params.Payload[1]);
        const f32 radius = params.Payload[2];
        const f32v2 s = sign(pos.xy);
        pos.xy += 0.5f * (dims - 1.f) * s;
        if (bool(region & RoundedQuadRegion_HorizontalEdge))
            pos[0] += (radius - 0.5f) * s[0];
        if (bool(region & RoundedQuadRegion_VerticalEdge))
            pos[1] += (radius - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_Capsule)
    {
        const f32 radius = params.Payload[1];
        const f32 diam = 2.f * radius;

        const f32v3 dims = f32v3(diam, params.Payload[0], diam);
        if (bool(region & CapsuleRegion_Body))
            pos *= dims;
        else
        {
            f32v3 dir = pos;
            const f32 s = sign(dir.y);
            dir.y -= 0.5f * s;
            pos += (radius - 0.5f) * normalize(dir);
            pos[1] += 0.5f * (dims[1] - 1.f) * s;
        }
    }
    else if (params.Shape == ParametricShape_RoundedBox)
    {
        const f32v3 dims = f32v3(params.Payload[0], params.Payload[1], params.Payload[2]);
        const f32 radius = params.Payload[3];
        pos += 0.5f * (dims - 1.f) * sign(pos);
        pos += (radius - 0.5f) * normal;
    }
    else if (params.Shape == ParametricShape_Torus)
    {
        const f32 iradius = params.Payload[0];
        const f32 oradius = params.Payload[1];

        const f32v2 dir = pos.xz;

        pos.xz += (oradius - 0.5f) * normalize(dir);
        pos += (iradius - 0.5f) * normal;
    }
    return pos;
}

bool CheckParametricFragment2D(f32v2 pos, const u32v2 shapeInfo)
{
    const u32 shape = shapeInfo[0];
    const u32 region = shapeInfo[1];
    if (shape == ParametricShape_Stadium && bool(region & StadiumRegion_Moon))
    {
        pos.y -= 0.5f * sign(pos.y);
        const f32 len = 4.f * dot(pos, pos);
        if (len > 1.f)
            return false;
    }
    else if (shape == ParametricShape_RoundedQuad && bool(region & RoundedQuadRegion_Moon))
    {
        pos -= 0.5f * sign(pos);
        const f32 len = 4.f * dot(pos, pos);
        if (len > 1.f)
            return false;
    }

    return true;
}

bool CheckParametricFragment3D(f32v3 pos, const u32v2 shapeInfo)
{
    const u32 shape = shapeInfo[0];
    const u32 region = shapeInfo[1];
    if (shape == ParametricShape_Stadium && bool(region & StadiumRegion_Moon))
    {
        f32v2 p = pos.xy;
        p.y -= 0.5f * sign(p.y);
        const f32 len = 4.f * dot(p, p);
        if (len > 1.f)
            return false;
    }
    else if (shape == ParametricShape_RoundedQuad && bool(region & RoundedQuadRegion_Moon))
    {
        f32v2 p = pos.xy;
        p -= 0.5f * sign(p);
        const f32 len = 4.f * dot(p, p);
        if (len > 1.f)
            return false;
    }
    return true;
}
