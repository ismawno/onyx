#pragma once

#include "onyx/core/math.hpp"

namespace Onyx
{
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)
struct alignas(16) Material
{
    u32 AlbedoFactor;
    f32 MetallicFactor;
    f32 RoughnessFactor;
    f32 NormalScale;
    f32 OcclusionStrength;

    u32 AlbedoTex;
    u32 MetallicRoughnessTex;
    u32 NormalTex;
    u32 OcclusionTex;
    u32 EmissiveTex;

    f32v3 EmissiveFactor;
    u32 Flags;
};
TKIT_COMPILER_WARNING_IGNORE_POP()

} // namespace Onyx
