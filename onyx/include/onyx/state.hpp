#pragma once

#include "onyx/alias.hpp"

namespace Onyx
{
using RenderState = u8;
constexpr RenderState CreateBitFieldMask(const RenderState bits, const RenderState offset)
{
    return ((1U << bits) - 1) << offset;
}
enum RenderStateBit : RenderState
{
    // unfortunately, fill and cull mode need to be first so that shadows can integrate that part of the key. so this is
    // order dependent
    // geometry + shadow
    FillMode_Offset = 0,
    FillMode_BitCount = 2,
    FillMode_None = 0U << FillMode_Offset,
    FillMode_Flat = 1U << FillMode_Offset,
    FillMode_Shaded = 2U << FillMode_Offset,
    FillMode_Mask = CreateBitFieldMask(FillMode_BitCount, FillMode_Offset),

    CullMode_Offset = 2,
    CullMode_BitCount = 2,
    CullMode_None = 0U << CullMode_Offset,
    CullMode_Back = 1U << CullMode_Offset,
    CullMode_Front = 2U << CullMode_Offset,
    CullMode_Mask = CreateBitFieldMask(CullMode_BitCount, CullMode_Offset),
    //

    // geometry only
    BlendMode_Offset = 4,
    BlendMode_BitCount = 1,
    BlendMode_None = 0U << BlendMode_Offset,
    BlendMode_OIT = 1U << BlendMode_Offset,
    BlendMode_Mask = CreateBitFieldMask(BlendMode_BitCount, BlendMode_Offset),

    OutlineMode_Offset = 5,
    OutlineMode_BitCount = 1,
    OutlineMode_Disabled = 0U << 5,
    OutlineMode_Enabled = 1U << 5,
    OutlineMode_Mask = CreateBitFieldMask(OutlineMode_BitCount, OutlineMode_Offset),
    //

    RenderState_GeometryBitCount = FillMode_BitCount + CullMode_BitCount + BlendMode_BitCount + OutlineMode_BitCount,
    RenderState_ShadowBitCount = FillMode_BitCount + CullMode_BitCount
};
} // namespace Onyx
