#pragma once

#include "onyx/core/dimension.hpp"

// NOTE(Isma): At some point ill have to handle user wanting to explicitly submit instance data buffers from gpu
// (without any cpu detours)
namespace Onyx
{
struct ArcData
{
    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;
};
struct FadeData
{
    u32 AngleOverflow;
    f32 Hollowness;
    f32 InnerFade;
    f32 OuterFade;
};

enum ParametricShape : u32
{
    ParametricShape_Stadium,
    ParametricShape_RoundedQuad,
    ParametricShape_Capsule,
    ParametricShape_RoundedBox,
    ParametricShape_Torus,
};

struct StadiumParameters
{
    f32 Height;
    f32 Radius;
};

struct RoundedQuadParameters
{
    f32 Width;
    f32 Height;
    f32 Radius;
};

struct CapsuleParameters
{
    f32 Height;
    f32 Radius;
};

struct RoundedBoxParameters
{
    f32 Width;
    f32 Height;
    f32 Length;
    f32 Radius;
};

struct TorusParameters
{
    f32 InnerRadius;
    f32 OuterRadius;
};

union InstanceParameters {
    StadiumParameters Stadium;
    RoundedQuadParameters RoundedQuad;
    CapsuleParameters Capsule;
    RoundedBoxParameters RoundedBox;
    TorusParameters Torus;
};

enum Geometry : u8
{
    Geometry_Circle,
    Geometry_Static,
    Geometry_Parametric,
    Geometry_Glyph,
    Geometry_Count,
};

enum LightType : u8
{
    Light_Point,
    Light_Directional,
    Light_Spot,
    Light_Count
};

using LightFlags = u32;
enum LightFlagBit : LightFlags
{
    LightFlag_CastShadows = 1 << 0,
    LightFlag_PCF = 1 << 1,
    LightFlag_PCSS = 1 << 2,
};

template <Dimension D> constexpr u32 LightTypeCount = D == D2 ? 2 : 3;

const char *ToString(Geometry geo);
const char *ToString(LightType light);

} // namespace Onyx
