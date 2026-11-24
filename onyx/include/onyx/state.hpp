#pragma once

#include "onyx/alias.hpp"
#include "tkit/utils/bit.hpp"

#define ONYX_VERTEX_HOOK_BIT_COUNT std::countr_zero(TKit::NextPowerOfTwo(u32(ONYX_MAX_VERTEX_HOOKS)))
#define ONYX_FRAGMENT_HOOK_BIT_COUNT std::countr_zero(TKit::NextPowerOfTwo(u32(ONYX_MAX_FRAGMENT_HOOKS)))

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
    // order dependent. this is needed because shadow bit field is a subset of the geometry's. the ones we want shared
    // are the lower end bits, so that the integer that designates the bit field is as little as possible geometry +
    // shadow
    FillMode_Offset = 0,
    FillMode_BitCount = 2,
    FillMode_None = 0U << FillMode_Offset,
    FillMode_Flat = 1U << FillMode_Offset,
    FillMode_Shaded = 2U << FillMode_Offset,
    FillMode_Mask = CreateBitFieldMask(FillMode_BitCount, FillMode_Offset),

    CullMode_Offset = FillMode_Offset + FillMode_BitCount,
    CullMode_BitCount = 2,
    CullMode_None = 0U << CullMode_Offset,
    CullMode_Back = 1U << CullMode_Offset,
    CullMode_Front = 2U << CullMode_Offset,
    CullMode_Mask = CreateBitFieldMask(CullMode_BitCount, CullMode_Offset),

#ifdef ONYX_ENABLE_HOOKS
    VertexHook_Offset = CullMode_Offset + CullMode_BitCount,
    VertexHook_BitCount = ONYX_VERTEX_HOOK_BIT_COUNT,
    VertexHook_Mask = CreateBitFieldMask(VertexHook_BitCount, VertexHook_Offset),
    //

    // geometry only
    BlendMode_Offset = VertexHook_Offset + VertexHook_BitCount,
#else
    BlendMode_Offset = CullMode_Offset + CullMode_BitCount,
#endif

    BlendMode_BitCount = 1,
    BlendMode_None = 0U << BlendMode_Offset,
    BlendMode_OIT = 1U << BlendMode_Offset,
    BlendMode_Mask = CreateBitFieldMask(BlendMode_BitCount, BlendMode_Offset),

    OutlineMode_Offset = BlendMode_Offset + BlendMode_BitCount,
    OutlineMode_BitCount = 1,
    OutlineMode_Disabled = 0U << 5,
    OutlineMode_Enabled = 1U << 5,
    OutlineMode_Mask = CreateBitFieldMask(OutlineMode_BitCount, OutlineMode_Offset),

#ifdef ONYX_ENABLE_HOOKS
    FragmentHook_Offset = OutlineMode_Offset + OutlineMode_BitCount,
    FragmentHook_BitCount = ONYX_FRAGMENT_HOOK_BIT_COUNT,
    FragmentHook_Mask = CreateBitFieldMask(FragmentHook_BitCount, FragmentHook_Offset),
#endif
    //

    RenderState_GeometryBitCount = FillMode_BitCount + CullMode_BitCount + ONYX_VERTEX_HOOK_BIT_COUNT +
                                   BlendMode_BitCount + OutlineMode_BitCount + ONYX_FRAGMENT_HOOK_BIT_COUNT,
    RenderState_ShadowBitCount = FillMode_BitCount + CullMode_BitCount + ONYX_VERTEX_HOOK_BIT_COUNT
};

enum RenderPass : u8
{
    RenderPass_Flat,
    RenderPass_Shaded,
    RenderPass_Shadow,
    RenderPass_Count
};

} // namespace Onyx
