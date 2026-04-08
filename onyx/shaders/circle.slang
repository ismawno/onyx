#pragma once

#include "common.slang"

typedef StructuredBuffer<CircleInstanceData2D, Std430DataLayout> CircleInstances2D;
typedef StructuredBuffer<CircleInstanceData3D, Std430DataLayout> CircleInstances3D;

static const f32v2 g_Positions[6] =
{
    f32v2(-0.5, -0.5),
    f32v2( 0.5, -0.5),
    f32v2(-0.5,  0.5),
    f32v2( 0.5, -0.5),
    f32v2( 0.5,  0.5),
    f32v2(-0.5,  0.5),
};

static const f32v2 g_TexCoords[6] =
{
    f32v2(0.0, 1.0),
    f32v2(1.0, 1.0),
    f32v2(0.0, 0.0),
    f32v2(1.0, 1.0),
    f32v2(1.0, 0.0),
    f32v2(0.0, 0.0)
};

struct ArcData
{
    f32v4 CosSin;
    u32 AngleOverflow;
    f32 Hollowness;
}

struct ArcDataPacked
{
    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;
    u32 AngleOverflow;
    f32 Hollowness;
}

struct FadeData
{
    f32 InnerFade;
    f32 OuterFade;
}

struct CircleInstanceData2D
{
    InstanceData2D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}
struct CircleInstanceData3D
{
    InstanceData3D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}

struct CircleBounds
{
    f32 Hollowness;
    f32 Radius;
}

CircleBounds GetCircleBounds(const f32v2 localPosition, const f32 hollowness)
{
    CircleBounds bounds;
    bounds.Hollowness = hollowness * hollowness;
    bounds.Radius = 4.0 * dot(localPosition, localPosition);
    return bounds;
}

bool CheckOutsideCircle(const CircleBounds bounds, const f32v2 localPosition, const ArcData arc)
{
    if (bounds.Radius > 1.f || bounds.Radius < bounds.Hollowness)
        return false;

    const f32 crs1 = localPosition.x * arc.CosSin[1] - localPosition.y * arc.CosSin[0];
    const f32 crs2 = localPosition.x * arc.CosSin[3] - localPosition.y * arc.CosSin[2];

    if (arc.AngleOverflow == 1)
    {
        if (crs1 > 0.f && crs2 < 0.f)
            return false;
    }
    else
    {
        if (crs1 > 0.f || crs2 < 0.f)
            return false;
    }
    return true;
}

f32 CircleFade(f32 alpha, const CircleBounds bounds, const f32v2 fade)
{
    const f32 innerFade = fade[0] * fade[0];
    const f32 outerFade = 1.0 - fade[1] * fade[1];
    const f32 shiftLen = (bounds.Radius - bounds.Hollowness) / (1.0 - bounds.Hollowness);

    if (shiftLen < innerFade)
        alpha *= shiftLen / innerFade;
    if (shiftLen > outerFade)
        alpha *= 1.0 - (shiftLen - outerFade) / (1.0 - outerFade);

    return clamp(alpha, 0.0, 1.0);
}

f32v2 ComputeCircleAlignment(const u32 alignment)
{
    const f32v2 bounds[9] = {f32v2(-0.5f, -0.5f), f32v2(-0.5f, 0.f), f32v2(-0.5f, 0.5f),
                             f32v2(0.f, -0.5f),   f32v2(0.f, 0.f),   f32v2(0.f, 0.5f),
                             f32v2(0.5f, -0.5f),  f32v2(0.5f, 0.f),  f32v2(0.5f, 0.5f)};
    u32 al0 = alignment & 0xFF;
    if (al0 == u32(Alignment_None))
        al0 = u32(Alignment_Center);

    u32 al1 = (alignment >> 8) & 0xFF;
    if (al1 == u32(Alignment_None))
        al1 = u32(Alignment_Center);

    return bounds[al0 * 3 + al1];
}
