#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
enum PrimitiveType : u8
{
    Primitive_StaticMesh,
    Primitive_Circle,
    Primitive_Count,
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
 * always be visible and on top of non-outlined shapes. The corresponding `DrawMode` is `Draw_Fill`.
 *
 * - `StencilPass_DoStencilWriteDoFill`: This pass will render the shape normally and write to the stencil buffer, which
 * corresponds to a shape being rendered both filled and with an outline. The corresponding `DrawMode` is `Draw_Fill`.
 *
 * - `StencilPass_DoStencilWriteNoFill`: This pass will only write to the stencil buffer and will not render the shape.
 * This step is necessary in case the user wants to render an outline only, without the shape being filled. The
 * corresponding `DrawMode` is `Draw_Outline`.
 *
 * - `StencilPass_DoStencilTestNoFill`: This pass will test the stencil buffer and render the shape only where the
 * stencil buffer is not set. The corresponding `DrawMode` is `Draw_Outline`.
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

} // namespace Onyx
