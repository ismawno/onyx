#pragma once

#include "onyx/asset/handle.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    static constexpr Dimension Dim = D2;
    u32 ColorFactor = 0xFFFFFFFF;
    Asset Sampler = NullHandle;
    Asset Texture = NullHandle;
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
    static constexpr Dimension Dim = D3;
    f32v3 EmissiveFactor{0.f};
    u32 AlbedoFactor = 0xFFFFFFFF;
    f32 MetallicFactor = 0.f;
    f32 RoughnessFactor = 0.5f;
    f32 OcclusionStrength = 1.f;
    f32 NormalScale = 1.f;

    TKit::FixedArray<Asset, TextureSlot_Count> Samplers{NullHandle, NullHandle, NullHandle, NullHandle, NullHandle};
    TKit::FixedArray<Asset, TextureSlot_Count> Textures{NullHandle, NullHandle, NullHandle, NullHandle, NullHandle};
};

const char *ToString(TextureSlot slot);

} // namespace Onyx
