#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
enum GeometryType : u8
{
    Geometry_Circle,
    Geometry_StaticMesh,
    Geometry_Count,
};

template <Dimension D> struct InstanceData;
template <> struct InstanceData<D2>
{
    f32v2 Basis1;
    f32v2 Basis2;
    f32v2 Basis3;
    u32 BaseColor;
    union {
        u32 TexIndex;
        f32 OutlineWidth;
    };
};

template <> struct InstanceData<D3>
{
    f32v4 Basis1;
    f32v4 Basis2;
    f32v4 Basis3;
    u32 BaseColor;
    union {
        u32 MatIndex;
        f32 OutlineWidth;
    };
};

template <Dimension D> struct CircleInstanceData
{
    InstanceData<D> BaseData;
    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;

    u32 AngleOverflow;
    f32 Hollowness;
    f32 InnerFade;
    f32 OuterFade;
};

struct DirectionalLight
{
    f32v3 Direction;
    f32 Intensity;
    u32 Color;
};

struct PointLight
{
    f32v3 Position;
    f32 Intensity;
    f32 Radius;
    u32 Color;
};

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
 * corresponding `DrawPass` is `DrawPass_Outline`.
 *
 * - `StencilPass_DoStencilTestNoFill`: This pass will test the stencil buffer and render the shape only where the
 * stencil buffer is not set. The corresponding `DrawPass` is `DrawPass_Outline`.
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
    DrawPass_Outline,
    DrawPass_Count
};

enum Shading : u8
{
    Shading_Unlit,
    Shading_Lit
};

constexpr DrawPass GetDrawMode(const StencilPass pass)
{
    if (pass == StencilPass_NoStencilWriteDoFill || pass == StencilPass_DoStencilWriteDoFill)
        return DrawPass_Fill;
    return DrawPass_Outline;
}

template <Dimension D> constexpr Shading GetShading(const DrawPass pass)
{
    if constexpr (D == D2)
        return Shading_Unlit;
    else
        return pass == DrawPass_Fill ? Shading_Lit : Shading_Unlit;
}
template <Dimension D> constexpr Shading GetShading(const StencilPass pass)
{
    return GetShading<D>(GetDrawMode(pass));
}

template <Shading Sh> struct PushConstantData;

template <> struct PushConstantData<Shading_Unlit>
{
    f32m4 ProjectionView;
};

template <> struct PushConstantData<Shading_Lit>
{
    f32m4 ProjectionView;
    f32v4 ViewPosition;
    f32v4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

} // namespace Onyx
