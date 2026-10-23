#pragma once

#include "onyx/handle.hpp"
#include "onyx/math.hpp"
#include "onyx/color.hpp"

namespace Onyx
{
template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    static constexpr Dimension Dim = D2;
    Color ColorFactor = Color_White;
    f32v2 TexOffset{0.f};
    f32v2 TexScale{1.f};
    Resource Sampler = NullHandle;
    Resource Texture = NullHandle;
    bool Occluder = false;
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
    Color EmissiveFactor{0.f};
    f32v2 TexOffset{0.f};
    f32v2 TexScale{1.f};
    u32 AlbedoFactor = 0xFFFFFFFF;
    f32 MetallicFactor = 0.f;
    f32 RoughnessFactor = 0.5f;
    f32 OcclusionStrength = 1.f;
    f32 NormalScale = 1.f;

    TKit::FixedArray<Resource, TextureSlot_Count> Samplers{NullHandle, NullHandle, NullHandle, NullHandle, NullHandle};
    TKit::FixedArray<Resource, TextureSlot_Count> Textures{NullHandle, NullHandle, NullHandle, NullHandle, NullHandle};
};

const char *ToString(TextureSlot slot);

} // namespace Onyx
