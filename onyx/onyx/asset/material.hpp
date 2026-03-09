#pragma once

#include "onyx/asset/texture.hpp"
#include "onyx/core/math.hpp"

namespace Onyx
{
using Material = u32;
constexpr Material NullMaterial = TKit::Limits<Material>::Max();

template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    u32 ColorFactor = 0xFFFFFFFF;
    Texture Texture = NullTexture;
    Sampler Sampler = NullSampler;
};

template <> struct MaterialData<D3>
{
    f32v3 EmissiveFactor{0.f};
    u32 AlbedoFactor = 0xFFFFFFFF;
    f32 MetallicFactor = 0.f;
    f32 RoughnessFactor = 0.5f;
    f32 OcclusionStrength = 1.f;
    f32 NormalScale = 1.f;
    Sampler Sampler = NullSampler;
    Texture AlbedoTex = NullTexture;
    Texture MetallicRoughnessTex = 0;
    Texture NormalTex = 0;
    Texture OcclusionTex = 0;
    Texture EmissiveTex = 0;
};

} // namespace Onyx
