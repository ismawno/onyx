#pragma once

#include "common.slang"

typedef StructuredBuffer<PointLight2D, Std430DataLayout> PointLights2D;
typedef StructuredBuffer<PointLight3D, Std430DataLayout> PointLights3D;

typedef StructuredBuffer<Material2D, Std430DataLayout> Materials2D;
typedef StructuredBuffer<Material3D, Std430DataLayout> Materials3D;

typedef StructuredBuffer<DirectionalLight, Std430DataLayout> DirectionalLights;

struct Material2D
{
    u32 ColorFactor;
    u32 Occluder;
    u32 Sampler;
    u32 Texture;
}

[UnscopedEnum]
enum TextureSlot : u8
{
    TextureSlot_Albedo,
    TextureSlot_MetallicRoughness,
    TextureSlot_Normal,
    TextureSlot_Occlusion,
    TextureSlot_Emissive,
    TextureSlot_Count,
};

struct Material3D
{
    f32 EmissiveFactorX;
    f32 EmissiveFactorY;
    f32 EmissiveFactorZ;

    u32 AlbedoFactor;
    f32 MetallicFactor;
    f32 RoughnessFactor;
    f32 OcclusionStrength;
    f32 NormalScale;

    u32 Samplers[TextureSlot_Count];
    u32 Textures[TextureSlot_Count];
}

[UnscopedEnum]
enum LightType : u8
{
    Light_Point = 0,
    Light_Directional = 1,
    Light_Count2D = 1,
    Light_Count3D = 2,
};

struct Range
{
    u32 Offset = 0;
    u32 Count = 0;
};

struct LightData2D
{
    Range LightRanges[Light_Count2D];
    f32 TexelSize;
    u32 AmbientColor;
    ViewMask ViewBit;
}

struct LightData3D
{
    f32 ViewPosX;
    f32 ViewPosY;
    f32 ViewPosZ;

    f32 ViewForwardX;
    f32 ViewForwardY;
    f32 ViewForwardZ;

    Range LightRanges[Light_Count3D];
    f32 TexelSizes[Light_Count3D];

    u32 AmbientColor;
    ViewMask ViewBit;
}

typedef u32 LightFlags;

[UnscopedEnum]
enum LightFlagBit : LightFlags
{
    LightFlag_CastsShadows = 1 << 0,
    LightFlag_PCF = 1 << 1,
};

struct PointLight2D
{
    f32 PosX;
    f32 PosY;
    f32 DirX;
    f32 DirY;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    f32 Angle;
    f32 Decay;
    f32 Extent;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
}

struct PointLight3D
{
    f32 PosX;
    f32 PosY;
    f32 PosZ;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
}

struct DirectionalLight
{
    Transform3D Cascades[ONYX_MAX_CASCADES];
    f32 Splits[ONYX_MAX_CASCADES];
    f32 DirX;
    f32 DirY;
    f32 DirZ;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    u32 CascadeCount;
    ViewMask ViewMask;
    LightFlags Flags;
}

struct AmbientLight
{
    u32 Color;
    ViewMask ViewMask;
}

struct MaterialInfo3D
{
    PointLights3D Points;
    DirectionalLights Directionals;
    f32v4 Albedo;
    f32v3 WorldPosition;
    f32v3 Normal;
    f32v3 Emissive;
    f32 Roughness;
    f32 Metallic;
    f32 Occlusion;
}

struct ResourceTable2D
{
    Materials2D Materials;
    SamplerState Samplers[ONYX_MAX_SAMPLERS];
    Texture2D Textures[ONYX_MAX_TEXTURES];
    PointLights2D PointLights;
    SamplerState ShadowSampler;
    Texture1D ShadowMaps[ONYX_MAX_TEXTURE_MAPS];
}

struct ResourceTable3D
{
    Materials3D Materials;
    SamplerState Samplers[ONYX_MAX_SAMPLERS];
    Texture2D Textures[ONYX_MAX_TEXTURES];
    PointLights3D PointLights;
    DirectionalLights DirLights;
    SamplerComparisonState ShadowSampler;
    TextureCube PointMaps[ONYX_MAX_TEXTURE_MAPS];
    Texture2DArray DirectionalMaps[ONYX_MAX_TEXTURE_MAPS];
}

#define PI 3.14159265

f32v3 GetLightColor2D(const LightData2D data, const f32v2 worldPosition, const PointLights2D plights, const ResourceTable2D resources)
{
    f32v3 diffuseColor = f32v3(0.0);
    const f32v4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    // TODO(Isma): Known artifacts: boundary between shadow and extent cone + boundary of the angle range (-pi, pi) in a shadow. Reproduce using shadows with extreme angles. Suggestion: use <= or >= in the (-pi, pi) remapping?

    for (u32 i = 0; i < data.LightRanges[Light_Point].Count; ++i)
    {
        const PointLight2D plight = plights[i + data.LightRanges[Light_Point].Offset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v2 direction = worldPosition - f32v2(plight.PosX, plight.PosY);
        const f32v2 lightDir = f32v2(plight.DirX, plight.DirY);

        const f32 dist2 = dot(direction, direction);
        const f32 dist = sqrt(dist2);

        const f32 alignment = 0.5f * (1.f + dot(direction / dist, lightDir));
        const f32 thres = 0.5f * (1.f + cos(PI * plight.Extent));

        const f32 contrib = f32(alignment >= thres) * saturate(1.f - plight.Decay * (1.f - alignment) / (1.f - thres));
        const f32v3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;

        const f32 radius = plight.LightRadius * plight.LightRadius;
        const f32 attenuation = contrib * contrib * radius / (dist2 + radius);
        const f32 intensity = attenuation * plight.Intensity;

        f32 shadow = 0.f;
        if (bool(plight.Flags & LightFlag_CastsShadows))
        {
            const f32 fragDepth = min(dist / plight.ShadowRadius, 1.f);
            const f32 ext = PI * plight.Extent;

            const f32 start = plight.Angle - ext;
            const f32 end = plight.Angle + ext;

            f32 angle = atan2(direction.y, direction.x);
            angle += 2 * PI * f32(angle < start);
            angle -= 2 * PI * f32(angle > end);
            const f32 uv = (angle - start) / (end - start);

            const u32 shadowIndex = plight.ShadowMapOffset + countbits(plight.ViewMask & (data.ViewBit - 1));

            const Texture1D map = resources.ShadowMaps[shadowIndex];
            const SamplerState smp = resources.ShadowSampler;

            if (bool(plight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSize;
                for (i32 x = -1; x <= 1; ++x)
                {
                    const f32 offset = x * tsize;
                    const f32 depth = map.Sample(smp, uv + offset).r;
                    shadow += f32(fragDepth <= depth);
                }
                shadow /= 3.f;
            }
            else
            {
                const f32 depth = resources.ShadowMaps[shadowIndex].Sample(resources.ShadowSampler, uv).r;
                shadow = f32(fragDepth <= depth);
            }
        }
        else
            shadow = 1.f;

        diffuseColor += intensity * lightColor * shadow;
    }

    return ambientColor.xyz * ambientColor.w + diffuseColor;
}

f32v4 ComputeMaterialColor2D(const f32v4 fragColor, const LightData2D light, const u32 matId, const f32v2 worldPos, const f32v2 uv, const ResourceTable2D resources, const f32 alpha)
{
    if (matId == NullAsset)
        return f32v4(fragColor.rgb, fragColor.a * alpha);

    const f32v3 lcolor = GetLightColor2D(light, worldPos, resources.PointLights, resources);

    const Material2D mat = resources.Materials[matId];
    const u32 sampId = GetAssetId(mat.Sampler);
    const u32 texId = GetAssetId(mat.Texture);

    if (sampId == NullAsset || texId == NullAsset)
        return fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);

    const f32v4 tex = resources.Textures[texId].Sample(resources.Samplers[sampId], uv);
    return tex * fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);
}

// pbr is ia generated
f32v3 GetLightColor3D(const LightData3D data, const MaterialInfo3D info, const ResourceTable3D resources)
{
    const f32v4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);

    const f32v3 vpos = f32v3(data.ViewPosX, data.ViewPosY, data.ViewPosZ);
    const f32v3 vforward = f32v3(data.ViewForwardX, data.ViewForwardY, data.ViewForwardZ);

    const f32v3 toView = vpos - info.WorldPosition;
    const f32 depth = -dot(toView, vforward);
    const f32v3 V = normalize(toView);

    const f32v3 baseColor = info.Albedo.xyz;
    const f32 metallic   = info.Metallic;
    const f32 roughness  = info.Roughness;
    const f32 alpha      = roughness * roughness;
    const f32 alpha2     = alpha * alpha;

    const f32v3 F0          = lerp(f32v3(0.04), baseColor, metallic);
    const f32v3 diffuseBase = baseColor * (1.0 - metallic);

    f32v3 Lo = f32v3(0.0);

    for (u32 i = 0; i < data.LightRanges[Light_Directional].Count; ++i)
    {
        const DirectionalLight dlight = info.Directionals[i + data.LightRanges[Light_Directional].Offset];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const f32v3 L = -f32v3(dlight.DirX, dlight.DirY, dlight.DirZ);
        const f32 NdotL = dot(info.Normal, L);

        if (NdotL <= 0.f) continue;

        const f32v3 H    = normalize(L + V);
        const f32 NdotV = max(dot(info.Normal, V), 1e-4);
        const f32 NdotH = max(dot(info.Normal, H), 0.0);
        const f32 VdotH = max(dot(V, H), 0.0);

        const f32 denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
        const f32 D     = alpha2 / (3.14159265 * denom * denom);
        const f32 k     = alpha * 0.5;
        const f32 G     = (NdotV / (NdotV * (1.0 - k) + k)) *
                            (NdotL / (NdotL * (1.0 - k) + k));
        const f32v3 F    = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        const f32v3 specular   = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        const f32v3 diffuse    = diffuseBase / 3.14159265;
        const f32v3 lightColor = unpackUnorm4x8ToFloat(dlight.Color).xyz;

        f32 shadow = 0.f;
        if (bool(dlight.Flags & LightFlag_CastsShadows))
        {
            u32 cascade = dlight.CascadeCount - 1;
            for (u32 j = 0; j < dlight.CascadeCount; ++j)
                if (depth < dlight.Splits[j])
                {
                    cascade = j;
                    break;
                }

            const u32 shadowIndex = dlight.ShadowMapOffset + countbits(dlight.ViewMask & (data.ViewBit - 1));
            const f32m4 pv = ComputeTransform3D(dlight.Cascades[cascade]);
            const f32v4 lightSpacePos = mul(f32v4(info.WorldPosition, 1.0), pv);
            const f32v3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
            const f32v2 shadowUV = projCoords.xy * 0.5 + 0.5;

            const Texture2DArray maps = resources.DirectionalMaps[shadowIndex];
            const SamplerComparisonState smp = resources.ShadowSampler;
            const f32 z = clamp(projCoords.z, 0.f, 1.f);

            if (bool(dlight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Directional];
                for (i32 x = -1; x <= 1; ++x)
                    for (i32 y = -1; y <= 1; ++y)
                    {
                        const f32v2 offset = f32v2(x, y) * tsize;
                        shadow += maps.SampleCmp(smp, f32v3(shadowUV + offset, cascade), z);
                    }
                shadow /= 9.f;
            }
            else
                shadow = maps.SampleCmp(smp, f32v3(shadowUV, cascade), z);
        }
        else
            shadow = 1.f;

        Lo += (diffuse * (1.0 - F) + specular) * lightColor * dlight.Intensity * NdotL * shadow;
    }

    for (u32 i = 0; i < data.LightRanges[Light_Point].Count; ++i)
    {
        const PointLight3D plight = info.Points[i + data.LightRanges[Light_Point].Offset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v3 dir  = f32v3(plight.PosX, plight.PosY, plight.PosZ) - info.WorldPosition;
        const f32 dist = length(dir);
        const f32v3 L    = dir / dist;
        const f32 NdotL = dot(info.Normal, L);
        if (NdotL <= 0.0) continue;

        const f32v3 H    = normalize(L + V);
        const f32 NdotV = max(dot(info.Normal, V), 1e-4);
        const f32 NdotH = max(dot(info.Normal, H), 0.0);
        const f32 VdotH = max(dot(V, H), 0.0);

        const f32 denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
        const f32 D     = alpha2 / (3.14159265 * denom * denom);
        const f32 k     = alpha * 0.5;
        const f32 G     = (NdotV / (NdotV * (1.0 - k) + k)) *
                            (NdotL / (NdotL * (1.0 - k) + k));
        const f32v3 F    = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        const f32v3 specular   = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        const f32v3 diffuse    = diffuseBase / 3.14159265;
        const f32  radius     = plight.LightRadius * plight.LightRadius;
        const f32  dirLen     = dot(dir, dir);
        const f32  atten      = radius / (dirLen + radius);
        const f32v3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;

        f32 shadow = 0.f;
        if (bool(plight.Flags & LightFlag_CastsShadows))
        {
            const f32 z = min(1.f, dist / plight.ShadowRadius);
            const u32 shadowIndex = plight.ShadowMapOffset + countbits(plight.ViewMask & (data.ViewBit - 1));

            const TextureCube map = resources.PointMaps[shadowIndex];
            const SamplerComparisonState smp = resources.ShadowSampler;

            if (bool(plight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Point];
                for (i32 x = -1; x <= 1; ++x)
                    for (i32 y = -1; y <= 1; ++y)
                    {
                        const f32v2 offset = f32v2(x, y) * tsize;
                        shadow += map.SampleCmp(smp, -dir, z);
                    }
                shadow /= 9.f;
            }
            else
                shadow = map.SampleCmp(smp, -dir, z);
        }
        else
            shadow = 1.f;


        Lo += (diffuse * (1.0 - F) + specular) * lightColor * atten * plight.Intensity * NdotL * shadow;
    }

    const f32v3 ambient = ambientColor.xyz * ambientColor.w * diffuseBase * info.Occlusion;

    return min(f32v3(1.0), ambient + Lo + info.Emissive);
}

f32v4 ComputeMaterialColor3D(const f32v4 fragColor, const LightData3D light, const u32 matId, const f32v3 worldPos, const f32v2 uv, const ResourceTable3D resources, const f32 alpha, const f32m3 TBN, const bool isFrontFacing)
{
    if (matId == NullAsset)
        return f32v4(fragColor.rgb, fragColor.a * alpha);

    const Material3D mat = resources.Materials[matId];

    const u32 albedoSamp = GetAssetId(mat.Samplers[TextureSlot_Albedo]);
    const u32 normalSamp = GetAssetId(mat.Samplers[TextureSlot_Normal]);
    const u32 mrSamp = GetAssetId(mat.Samplers[TextureSlot_MetallicRoughness]);
    const u32 occSamp = GetAssetId(mat.Samplers[TextureSlot_Occlusion]);
    const u32 emissiveSamp = GetAssetId(mat.Samplers[TextureSlot_Emissive]);

    const u32 albedoTex = GetAssetId(mat.Textures[TextureSlot_Albedo]);
    const u32 normalTex = GetAssetId(mat.Textures[TextureSlot_Normal]);
    const u32 mrTex = GetAssetId(mat.Textures[TextureSlot_MetallicRoughness]);
    const u32 occTex = GetAssetId(mat.Textures[TextureSlot_Occlusion]);
    const u32 emissiveTex = GetAssetId(mat.Textures[TextureSlot_Emissive]);

    MaterialInfo3D info;
    info.Points = resources.PointLights;
    info.Directionals = resources.DirLights;
    info.Albedo = unpackUnorm4x8ToFloat(mat.AlbedoFactor);
    info.WorldPosition = worldPos;
    info.Emissive = f32v3(mat.EmissiveFactorX, mat.EmissiveFactorY, mat.EmissiveFactorZ);
    info.Roughness = mat.RoughnessFactor;
    info.Metallic = mat.MetallicFactor;
    info.Occlusion = 1.0;

    // Albedo
    if (albedoSamp != NullAsset && albedoTex != NullAsset)
        info.Albedo *= resources.Textures[albedoTex].Sample(resources.Samplers[albedoSamp], uv);

    // Normal map
    if (normalSamp != NullAsset && normalTex != NullAsset)
    {
        f32v3 sampledNormal = resources.Textures[normalTex].Sample(resources.Samplers[normalSamp], uv).xyz;
        sampledNormal = sampledNormal * 2.0 - 1.0; // unpack from [0,1] to [-1,1]
        sampledNormal.xy *= mat.NormalScale;
        info.Normal = normalize(mul(sampledNormal, TBN)); // requires TBN matrix in FragInput
    }
    else
        info.Normal = TBN[2];

    if (!isFrontFacing)
        info.Normal = -info.Normal;

    // Metallic/Roughness (glTF convention: G = roughness, B = metallic)
    if (mrSamp != NullAsset && mrTex != NullAsset)
    {
        const f32v4 mr = resources.Textures[mrTex].Sample(resources.Samplers[mrSamp], uv);
        info.Roughness *= mr.g;
        info.Metallic *= mr.b;
    }

    // Occlusion (R channel)
    if (occSamp != NullAsset && occTex != NullAsset)
    {
        const f32 sampledOcclusion = resources.Textures[occTex].Sample(resources.Samplers[occSamp], uv).r;
        info.Occlusion = 1.0 + mat.OcclusionStrength * (sampledOcclusion - 1.0);
    }

    // Emissive
    if (emissiveSamp != NullAsset && emissiveTex != NullAsset)
        info.Emissive *= resources.Textures[emissiveTex].Sample(resources.Samplers[emissiveSamp], uv).rgb;

    const f32v3 lcolor = GetLightColor3D(light, info, resources);
    return fragColor * f32v4(lcolor, info.Albedo.w * alpha);
}
