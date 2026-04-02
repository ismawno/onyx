#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
template <Dimension D> struct StaticInstanceData;
template <> struct StaticInstanceData<D2>
{
    f32v2 Column0;
    f32v2 Column1;
    f32v2 Column3;
    u32 FillColor;
    u32 OutlineColor;
    u32 DepthCounter;
    u32 MatHandle;
    f32 OutlineWidth;
};

template <> struct StaticInstanceData<D3>
{
    f32v4 Row0;
    f32v4 Row1;
    f32v4 Row2;
    u32 FillColor;
    u32 OutlineColor;
    u32 MatHandle;
    f32 OutlineWidth;
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
    StaticInstanceData<D> Data;
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
    StaticInstanceData<D> Data;
    InstanceParameters Parameters;
    ParametricShape Shape;
};

template <Dimension D> struct GlyphInstanceData
{
    StaticInstanceData<D> Data;
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

template <Dimension D> struct PointLightData
{
    f32v<D> Position;
    f32 Intensity;
    f32 Radius;
    u32 Color;
    ViewMask ViewMask;
};

struct DirectionalLightData
{
    f32v3 Direction;
    f32 Intensity;
    u32 Color;
    ViewMask ViewMask;
};

template <Dimension D> constexpr u32 LightTypeCount = D == D2 ? 1 : 2;

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

/**
 * @brief The `StencilPass` enum represents a grouping of pipelines with slightly different settings that all renderers
 * use.
 *
 * To support nice outlines, especially in 3D, the stencil buffer can be used to re-render the same shape
 * slightly scaled only in places where the stencil buffer has not been set. Generally, only two passes would be
 * necessary, but in this implementation four are used.
 *
 * - `StencilPass_NoStencilWriteDoFill`: This pass will render the shape normally and corresponds to a shape being
 * rendered without an outline, thus not writing to the stencil buffer. This is important so that other shapes having
 * outlines can have theirs drawn on top of objects that do not have an outline. This way, an object's outline will
 * always be visible and on top of non-outlined shapes. The corresponding `DrawPass` is `DrawPass_Fill`.
 *
 * - `StencilPass_DoStencilWriteDoFill`: This pass will render the shape normally and write to the stencil buffer, which
 * corresponds to a shape being rendered both filled and with an outline. The corresponding `DrawPass` is
 * `DrawPass_Fill`.
 *
 * - `StencilPass_DoStencilWriteNoFill`: This pass will only write to the stencil buffer and will not render the shape.
 * This step is necessary in case the user wants to render an outline only, without the shape being filled. The
 * corresponding `DrawPass` is `DrawPass_Stencil`.
 *
 * - `StencilPass_DoStencilTestNoFill`: This pass will test the stencil buffer and render the shape only where the
 * stencil buffer is not set. The corresponding `DrawPass` is `DrawPass_Stencil`.
 *
 */
enum StencilPass : u8
{
    StencilPass_NoStencilWriteDoFill,
    StencilPass_DoStencilWriteDoFill,
    StencilPass_DoStencilWriteNoFill,
    StencilPass_DoStencilTestNoFill,
    StencilPass_Count
};
enum DrawPass : u8
{
    DrawPass_Fill,
    DrawPass_Stencil,
    DrawPass_Count
};

enum DrawMode : u8
{
    DrawMode_Fill,
    DrawMode_Stencil,
    DrawMode_FillStencil,
    DrawMode_Count,
    DrawMode_None = DrawMode_Count,
};

const char *ToString(Geometry geo);
const char *ToString(LightType light);
const char *ToString(StencilPass pass);
const char *ToString(DrawPass pass);
const char *ToString(DrawMode mode);

constexpr DrawPass GetDrawPass(const StencilPass pass)
{
    if (pass == StencilPass_NoStencilWriteDoFill || pass == StencilPass_DoStencilWriteDoFill)
        return DrawPass_Fill;
    return DrawPass_Stencil;
}

struct LightRange
{
    u32 LightOffset = 0;
    u32 LightCount = 0;
};

template <Dimension D> struct FillPushConstantData;

template <> struct FillPushConstantData<D2>
{
    f32m4 ProjectionView;
    TKit::FixedArray<LightRange, LightTypeCount<D2>> LightRanges{};
    u32 AmbientColor;
    ViewMask ViewBit;
};

template <> struct FillPushConstantData<D3>
{
    f32m4 ProjectionView;
    f32v4 ViewPosition;
    TKit::FixedArray<LightRange, LightTypeCount<D3>> LightRanges{};
    u32 AmbientColor;
    ViewMask ViewBit;
};

struct StencilPushConstantData
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
};

} // namespace Onyx
