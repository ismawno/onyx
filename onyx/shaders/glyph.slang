#pragma once

#include "common.slang"

typedef StructuredBuffer<GlyphInstanceData2D, Std430DataLayout> GlyphInstances2D;
typedef StructuredBuffer<GlyphInstanceData3D, Std430DataLayout> GlyphInstances3D;

struct GlyphInstanceData2D
{
    InstanceData2D Data;
    u32 BoundsHandle;
    u32 SamplerHandle;
    u32 AtlasHandle;
}
struct GlyphInstanceData3D
{
    InstanceData3D Data;
    u32 BoundsHandle;
    u32 SamplerHandle;
    u32 AtlasHandle;
}

// this is ai generated
f32 ComputeMTSDFCoverage(
    const SamplerState samplers[ONYX_MAX_SAMPLERS],
    const Texture2D textures[ONYX_MAX_TEXTURES],
    const u32 samplerId,
    const u32 atlasId,
    const f32v2 uv)
{
    const f32v4 mtsdf = textures[atlasId].Sample(samplers[samplerId], uv);

    // MSDF: median of RGB — sharp at high resolution
    const f32 median = max(min(mtsdf.r, mtsdf.g), min(max(mtsdf.r, mtsdf.g), mtsdf.b));
    // SDF: alpha channel — stable at low resolution / small sizes
    const f32 sdf = mtsdf.a;

    // Screen-space pixel range: how many screen pixels cover one distance-field unit.
    // unitRange = pxRange / texSize = 4.0 / 512.0 ≈ 0.0078125
    const f32v2 screenTexSize = f32v2(0.0078125) / fwidth(uv);
    const f32 screenPxRange = max(0.5 * dot(screenTexSize, f32v2(1.0)), 1.0);

    // Blend weight: 0 = full MSDF, 1 = full SDF.
    // Below ~2 screen pixels per range unit, MSDF starts showing artifacts.
    const f32 threshold_lo = 1.5;
    const f32 threshold_hi = 3.5;
    const f32 blendT = 1.0 - saturate((screenPxRange - threshold_lo) / (threshold_hi - threshold_lo));

    const f32 signal = lerp(median, sdf, blendT);

    // Smoothing: 1 / screenPxRange gives a half-pixel AA band,
    // clamped so glyphs stay readable at very small sizes.
    const f32 smoothing = clamp(0.5 / screenPxRange, 0.005, 0.30);

    return smoothstep(0.5 - smoothing, 0.5 + smoothing, signal);
}
