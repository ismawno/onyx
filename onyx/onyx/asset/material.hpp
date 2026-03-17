#pragma once

#include "onyx/asset/sampler.hpp"
#include "onyx/asset/texture.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
using Material = Handle;
constexpr Material NullMaterial = NullHandle;

template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    u32 ColorFactor = 0xFFFFFFFF;
    Sampler Sampler = NullSampler;
    Texture Texture = NullTexture;
};

enum TextureSlot : u8
{
    TextureSlot_Albedo,
    TextureSlot_MetallicRoughness,
    TextureSlot_Normal,
    TextureSlot_Occlusion,
    TextureSlot_Emissive,
    TextureSlot_Count,
};

template <> struct MaterialData<D3>
{
    f32v3 EmissiveFactor{0.f};
    u32 AlbedoFactor = 0xFFFFFFFF;
    f32 MetallicFactor = 0.f;
    f32 RoughnessFactor = 0.5f;
    f32 OcclusionStrength = 1.f;
    f32 NormalScale = 1.f;

    TKit::FixedArray<Sampler, TextureSlot_Count> Samplers{NullSampler, NullSampler, NullSampler, NullSampler,
                                                          NullSampler};
    TKit::FixedArray<Texture, TextureSlot_Count> Textures{NullTexture, NullTexture, NullTexture, NullTexture,
                                                          NullTexture};
};

} // namespace Onyx
