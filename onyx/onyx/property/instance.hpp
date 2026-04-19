#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
template <Dimension D> struct TransformData;

template <> struct TransformData<D2>
{
    f32v2 Column0;
    f32v2 Column1;
    f32v2 Column3;
};

template <> struct TransformData<D3>
{
    f32v4 Row0;
    f32v4 Row1;
    f32v4 Row2;
};

template <Dimension D> TransformData<D> CreateTransformData(const f32m<D> &transform)
{
    TransformData<D> data;
    if constexpr (D == D2)
    {
        data.Column0 = f32v2{transform[0]};
        data.Column1 = f32v2{transform[1]};
        data.Column3 = f32v2{transform[2]};
    }
    else
    {
        data.Row0 = f32v4{transform[0][0], transform[1][0], transform[2][0], transform[3][0]};
        data.Row1 = f32v4{transform[0][1], transform[1][1], transform[2][1], transform[3][1]};
        data.Row2 = f32v4{transform[0][2], transform[1][2], transform[2][2], transform[3][2]};
    }
    return data;
}

template <Dimension D> struct InstanceData
{
    TransformData<D> Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
    f32 OutlineWidth;
};
template <> struct InstanceData<D2>
{
    TransformData<D2> Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
    f32 OutlineWidth;
    u32 DepthCounter;
};

template <Dimension D> struct StaticInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
};

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

template <Dimension D> struct CircleInstanceData
{
    InstanceData<D> Data;
    ArcData Arc;
    FadeData Fade;
};

TKIT_YAML_SERIALIZE_DECLARE_ENUM(ParametricShape)
TKIT_REFLECT_DECLARE_ENUM(ParametricShape)
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

template <Dimension D> struct ParametricInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
    InstanceParameters Parameters;
    ParametricShape Shape;
};

template <Dimension D> struct GlyphInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
    u32 SamplerHandle;
    u32 AtlasHandle;
};

TKIT_YAML_SERIALIZE_DECLARE_ENUM(Geometry)
TKIT_YAML_SERIALIZE_DECLARE_ENUM(LightType)
TKIT_REFLECT_DECLARE_ENUM(Geometry)
TKIT_REFLECT_DECLARE_ENUM(LightType)
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
    Light_Count
};

using LightFlags = u32;
enum LightFlagBit : LightFlags
{
    LightFlag_CastsShadows = 1 << 0,
    LightFlag_PCF = 1 << 1,
};

struct Range
{
    u32 Offset = 0;
    u32 Count = 0;
};

template <Dimension D> u32 GetInstanceSize(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return sizeof(CircleInstanceData<D>);
    case Geometry_Static:
        return sizeof(StaticInstanceData<D>);
    case Geometry_Parametric:
        return sizeof(ParametricInstanceData<D>);
    case Geometry_Glyph:
        return sizeof(GlyphInstanceData<D>);
    default:
        TKIT_FATAL("[ONYX][INSTANCE] Unrecognized geometry type");
        return 0;
    }
}

const char *ToString(Geometry geo);
const char *ToString(LightType light);

template <Dimension D> struct PointLightData
{
    f32v<D> Position;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
};

#define ONYX_MAX_CASCADES 4

struct DirectionalLightData
{
    TKit::FixedArray<TransformData<D3>, ONYX_MAX_CASCADES> Cascades;
    TKit::FixedArray<f32, ONYX_MAX_CASCADES> Splits;
    f32v3 Direction;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    u32 CascadeCount;
    ViewMask ViewMask;
    LightFlags Flags;
};

template <Dimension D> constexpr u32 LightTypeCount = D == D2 ? 1 : 2;

template <Dimension D> struct FillPushConstantData;

template <> struct FillPushConstantData<D2>
{
    f32m4 ProjectionView;
    TKit::FixedArray<Range, LightTypeCount<D2>> LightRanges{};
    f32 TexelSize;
    u32 AmbientColor;
    ViewMask ViewBit;
};

template <> struct FillPushConstantData<D3>
{
    f32m4 ProjectionView;
    f32v3 ViewPosition;
    f32v3 ViewForward;
    TKit::FixedArray<Range, LightTypeCount<D3>> LightRanges{};
    TKit::FixedArray<f32, LightTypeCount<D3>> TexelSizes{};
    u32 AmbientColor;
    ViewMask ViewBit;
};

struct StencilPushConstantData
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
};

template <Dimension D> struct ShadowPushConstantData
{
    f32m4 LightProjection;
};
template <> struct ShadowPushConstantData<D3>
{
    f32m4 LightProjection;
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
};

struct DistancePushConstantData
{
    u32 OcclusionMapIndex;
    u32 OcclusionResolution;
    u32 ShadowMapIndex;
    u32 ShadowResolution;
    f32 DistanceBias;
};

} // namespace Onyx
