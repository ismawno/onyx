#pragma once

#include "common.slang"

typedef StructuredBuffer<PointLight2D, Std430DataLayout> PointLights2D;
typedef StructuredBuffer<PointLight3D, Std430DataLayout> PointLights3D;

typedef StructuredBuffer<Material2D, Std430DataLayout> Materials2D;
typedef StructuredBuffer<Material3D, Std430DataLayout> Materials3D;

typedef StructuredBuffer<DirectionalLight2D, Std430DataLayout> DirectionalLights2D;
typedef StructuredBuffer<DirectionalLight3D, Std430DataLayout> DirectionalLights3D;

typedef StructuredBuffer<SpotLight, Std430DataLayout> SpotLights;

struct Material2D
{
    u32 ColorFactor;
    u32 Occluder;
    f32 TexOffset[2];
    f32 TexScale[2];
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
    f32 EmissiveFactor[3];

    f32 TexOffset[2];
    f32 TexScale[2];

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
    Light_Spot = 2,
    Light_Count2D = 2,
    Light_Count3D = 3,
};

typedef u32 ShadedFlags;

[UnscopedEnum]
enum ShadedFlagBit : ShadedFlags
{
    ShadedFlag_Shadows = 1U << 0,
};

struct LightData2D
{
    u32 LightRanges[Light_Count2D];
    f32 TexelSizes[Light_Count2D];
    u32 AmbientColor;
    ShadedFlags Flags;
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

    u32 LightRanges[Light_Count3D];
    f32 TexelSizes[Light_Count3D];

    u32 AmbientColor;
    ShadedFlags Flags;
    ViewMask ViewBit;
}

typedef u32 LightFlags;

[UnscopedEnum]
enum LightFlagBit : LightFlags
{
    LightFlag_CastShadows = 1U << 0,
    LightFlag_PCF = 1U << 1,
    LightFlag_PCSS = 1U << 2,
};

struct PointLight2D
{
    f32 PosX;
    f32 PosY;

    f32 DirX;
    f32 DirY;

    f32 LightSize;
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

    f32 LightSize;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    u32 Color;
    u32 ShadowMapOffset;

    ViewMask ViewMask;
    LightFlags Flags;
}

struct DirectionalLight2D
{
    Transform2D ProjectionView;

    f32 DirX;
    f32 DirY;

    f32 LightOffset;
    f32 LightExtent;
    f32 ShadowExtent;
    f32 Decay;
    f32 TanAngleSize;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;

    ViewMask ViewMask;
    LightFlags Flags;
}

struct CascadeData
{
    Transform3D ProjectionView;

    f32 ShadowInvSizeX;
    f32 ShadowInvSizeY;

    f32 DepthRange;
    f32 Split;
}

struct DirectionalLight3D
{
    CascadeData Cascades[ONYX_MAX_CASCADES];

    f32 DirX;
    f32 DirY;
    f32 DirZ;

    f32 OffsetX;
    f32 OffsetY;
    f32 OffsetZ;

    f32 ExtentX;
    f32 ExtentY;

    f32 PlaneVec1X;
    f32 PlaneVec1Y;
    f32 PlaneVec1Z;

    f32 PlaneVec2X;
    f32 PlaneVec2Y;
    f32 PlaneVec2Z;

    f32 Decay;
    f32 TanAngleSize;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    u32 CascadeCount;
    u32 CascadeEnable;

    ViewMask ViewMask;
    LightFlags Flags;
}

struct PerspectiveTransform
{
    f32 Elm00, Elm01, Elm02, Elm03;
    f32 Elm10, Elm11, Elm12, Elm13;
    f32 Elm20, Elm21, Elm22, Elm23;
    f32 Elm30, Elm31, Elm32, Elm33;
}

struct SpotLight
{
    PerspectiveTransform ProjectionView;

    f32 PosX;
    f32 PosY;
    f32 PosZ;

    f32 DirX;
    f32 DirY;
    f32 DirZ;

    f32 ShadowInvSizeX;
    f32 ShadowInvSizeY;

    f32 Near;
    f32 Far;

    f32 LightSize;
    f32 CosHalfPov;
    f32 LightRange;
    f32 Decay;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;

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
    DirectionalLights2D DirLights;

    SamplerState ShadowSampler;
    SamplerComparisonState ShadowCompareSampler;
    Texture1D<f32> ShadowMaps[ONYX_MAX_SHADOW_OCCLUSION_MAP_SIZE];
}

struct ResourceTable3D
{
    Materials3D Materials;
    SamplerState Samplers[ONYX_MAX_SAMPLERS];
    Texture2D Textures[ONYX_MAX_TEXTURES];

    PointLights3D PointLights;
    DirectionalLights3D DirLights;
    SpotLights SpLights;

    SamplerState ShadowSampler;
    SamplerComparisonState ShadowCompareSampler;
    TextureCube<f32> PointMaps[ONYX_MAX_TEXTURE_MAPS];
    Texture2DArray<f32> DirectionalMaps[ONYX_MAX_TEXTURE_MAPS];
    Texture2D<f32> SpotMaps[ONYX_MAX_TEXTURE_MAPS];
}

f32m3 ConstructReducedTransform(const Transform2D transform)
{
    return f32m3(
        f32v3(transform.Elm00, transform.Elm01, 0.f),
        f32v3(transform.Elm10, transform.Elm11, 0.f),
        f32v3(transform.Elm30, transform.Elm31, 1.f)
    );
}

f32m4 ConstructPerspectiveTransform(const PerspectiveTransform transform)
{
    return f32m4(
        f32v4(transform.Elm00, transform.Elm01, transform.Elm02, transform.Elm03),
        f32v4(transform.Elm10, transform.Elm11, transform.Elm12, transform.Elm13),
        f32v4(transform.Elm20, transform.Elm21, transform.Elm22, transform.Elm23),
        f32v4(transform.Elm30, transform.Elm31, transform.Elm32, transform.Elm33)
    );
}

#define PI 3.14159265
#define POISSON_DISK_SAMPLES 16
#define POISSON_LINE_SAMPLES 8

// 16-point Poisson Disk Samples (unit disk, min distance ~0.32)
static const f32v2 poissonDisk[POISSON_DISK_SAMPLES] = {
    f32v2( -0.94201624, -0.39906216 ),
    f32v2( 0.94558609, -0.76890725 ),
    f32v2( -0.094184101, -0.92938870 ),
    f32v2( 0.34495938, 0.29387760 ),
    f32v2( -0.91588581, 0.45771432 ),
    f32v2( -0.81544232, -0.87912464 ),
    f32v2( -0.38277543, 0.27676845 ),
    f32v2( 0.97484398, 0.75648379 ),
    f32v2( 0.44323325, -0.97511554 ),
    f32v2( 0.53742981, -0.47373420 ),
    f32v2( -0.26496911, -0.41893023 ),
    f32v2( 0.79197514, 0.19090188 ),
    f32v2( -0.24188840, 0.99706507 ),
    f32v2( -0.81409955, 0.91437590 ),
    f32v2( 0.19984126, 0.78641367 ),
    f32v2( 0.14383161, -0.14100790 )
};


static const f32 poissonLine[POISSON_LINE_SAMPLES] = {
    -0.932, -0.687, -0.451, -0.178, 0.094, 0.358, 0.612, 0.876,
};

struct LightRange
{
    u32 Offset;
    u32 Count;
};

LightRange GetRange(const u32 range)
{
    LightRange r;
    r.Offset = range >> 16;
    r.Count = range & 0xFFFF;
    return r;
}

#define ONYX_MAX_SEARCH_RADIUS_TEXELS 16.f

[vk::constant_id(0)]
const u32 depth2D = 0;

f32 SampleShadow(const Texture1D<f32> map, const SamplerComparisonState csmp, const SamplerState smp, const f32 uv, const f32 fragDepth)
{
    if (depth2D == 0)
        return f32(map.Sample(smp, uv) >= fragDepth);
    else
        return map.SampleCmp(csmp, uv, fragDepth);
}

f32v3 ComputeLightColor(const LightData2D data, const f32v2 worldPosition, const ResourceTable2D resources)
{
    f32v3 diffuseColor = f32v3(0.0);
    const f32v4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);

    const LightRange prg = GetRange(data.LightRanges[Light_Point]);
    for (u32 i = 0; i < prg.Count; ++i)
    {
        const PointLight2D plight = resources.PointLights[i + prg.Offset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v2 direction = worldPosition - f32v2(plight.PosX, plight.PosY);
        const f32v2 lightDir = f32v2(plight.DirX, plight.DirY);

        const f32 dist2 = dot(direction, direction);
        const f32 dist = sqrt(dist2);

        const f32 alignment = 0.5f * (1.f + dot(direction / dist, lightDir));
        const f32 thres = 0.5f * (1.f + cos(PI * plight.Extent));

        const f32 atten = f32(alignment >= thres) * saturate(1.f - plight.Decay * (1.f - alignment) / (1.f - thres));
        const f32v3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;

        const f32 radius = plight.LightRadius * plight.LightRadius;
        const f32 attenuation = atten * atten * radius / (dist2 + radius);
        const f32 intensity = attenuation * plight.Intensity;

        f32 shadow = 1.f;
        if (bool(data.Flags & ShadedFlag_Shadows) && bool(plight.Flags & LightFlag_CastShadows))
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

            const Texture1D<f32> map = resources.ShadowMaps[shadowIndex];
            const SamplerState smp = resources.ShadowSampler;
            const SamplerComparisonState csmp = resources.ShadowCompareSampler;

            if (bool(plight.Flags & LightFlag_PCSS))
            {
                const SamplerState smp = resources.ShadowSampler;
                const f32 tsize = data.TexelSizes[Light_Point];
                const f32 far = plight.ShadowRadius;

                const f32 txPerWorld = 0.5f / far;
                const f32 near = ONYX_POINT_LIGHT_NEAR_RADIUS_FACTOR * far;

                const f32 searchRadius = clamp(plight.LightSize * (dist - near) * txPerWorld / near, tsize, ONYX_MAX_SEARCH_RADIUS_TEXELS * tsize);

                u32 count = 0;
                f32 blockerDepth = 0.f;
                for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                {
                    const f32 offset = poissonLine[i] * searchRadius;
                    const f32 depth = map.Sample(smp, uv + offset);
                    if (depth < fragDepth)
                    {
                        ++count;
                        blockerDepth += depth;
                    }
                }
                if (count != 0)
                {
                    blockerDepth = blockerDepth * far / count;
                    const f32 penumbra = plight.LightSize * (dist - blockerDepth) * txPerWorld / blockerDepth;
                    shadow = 0.f;
                    for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                    {
                        const f32 offset = poissonLine[i] * penumbra;
                        shadow += SampleShadow(map, csmp, smp, uv + offset, fragDepth);
                    }
                    shadow /= POISSON_LINE_SAMPLES;
                }
            }
            else if (bool(plight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Point];
                shadow = 0.f;
                for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                {
                    const f32 offset = poissonLine[i] * tsize;
                    shadow += SampleShadow(map, csmp, smp, uv + offset, fragDepth);
                }
                shadow /= POISSON_LINE_SAMPLES;
            }
            else
                shadow = SampleShadow(map, csmp, smp, uv, fragDepth);
        }

        diffuseColor += intensity * lightColor * shadow;
    }

    const LightRange drg = GetRange(data.LightRanges[Light_Directional]);
    for (u32 i = 0; i < drg.Count; ++i)
    {
        const DirectionalLight2D dlight = resources.DirLights[i + drg.Offset];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const f32v2 dir = f32v2(dlight.DirX, dlight.DirY);
        const f32v2 normal = f32v2(-dir.y, dir.x);

        const f32 separation = abs(dot(worldPosition, normal) - dlight.LightOffset);
        const f32 thres = dlight.LightExtent;

        const f32 atten = f32(separation <= thres) * saturate(1.f - dlight.Decay * separation / thres);
        const f32 intensity = atten * dlight.Intensity;

        f32 shadow = 1.f;
        if (bool(data.Flags & ShadedFlag_Shadows) && bool(dlight.Flags & LightFlag_CastShadows))
        {
            const f32m3 pv = ConstructReducedTransform(dlight.ProjectionView);
            const f32v2 projCoords = mul(f32v3(worldPosition, 1.f), pv).xy;
            const f32v2 remapCoords = saturate(0.5f * (1.f + projCoords));

            const u32 shadowIndex = dlight.ShadowMapOffset + countbits(dlight.ViewMask & (data.ViewBit - 1));

            const Texture1D<f32> map = resources.ShadowMaps[shadowIndex];
            const SamplerState smp = resources.ShadowSampler;
            const SamplerComparisonState csmp = resources.ShadowCompareSampler;

            const f32 fragDepth = remapCoords.x;
            const f32 uv = remapCoords.y;

            if (bool(dlight.Flags & LightFlag_PCSS))
            {
                const f32 tsize = data.TexelSizes[Light_Directional];
                const f32 tn = dlight.TanAngleSize;
                const f32 txPerWorld = 0.5f / dlight.LightExtent;
                const f32 searchRadius = clamp(fragDepth * dlight.ShadowExtent * tn * txPerWorld, tsize, ONYX_MAX_SEARCH_RADIUS_TEXELS * tsize);

                u32 count = 0;
                f32 blockerDepth = 0.f;
                for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                {
                    const f32 offset = poissonLine[i] * searchRadius;
                    const f32 depth = map.Sample(smp, uv + offset);
                    if (depth < fragDepth)
                    {
                        ++count;
                        blockerDepth += depth;
                    }
                }
                if (count != 0)
                {
                    blockerDepth /= count;
                    const f32 penumbra = tn * (fragDepth - blockerDepth) * dlight.ShadowExtent * txPerWorld;
                    shadow = 0.f;
                    for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                    {
                        const f32 offset = poissonLine[i] * penumbra;
                        shadow += SampleShadow(map, csmp, smp, uv + offset, fragDepth);
                    }
                    shadow /= POISSON_LINE_SAMPLES;
                }
            }
            else if (bool(dlight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Directional];
                shadow = 0.f;
                for (u32 i = 0; i < POISSON_LINE_SAMPLES; ++i)
                {
                    const f32 offset = poissonLine[i] * tsize;
                    shadow += SampleShadow(map, csmp, smp, uv + offset, fragDepth);
                }
                shadow /= POISSON_LINE_SAMPLES;
            }
            else
                shadow = SampleShadow(map, csmp, smp, uv, fragDepth);
        }

        const f32v3 lightColor = unpackUnorm4x8ToFloat(dlight.Color).xyz;
        diffuseColor += intensity * lightColor * shadow;
    }

    return ambientColor.xyz * ambientColor.w + diffuseColor;
}

f32v3 ComputeLightContribution(const MaterialInfo3D info, const f32v3 L, const f32v3 V, const f32v3 F0, const f32v3 diffuseBase, const f32 alpha, const f32 alpha2, const u32 color)
{
    const f32 NdotL = dot(info.Normal, L);

    if (NdotL <= 0.f) return f32v3(0.f);

    const f32v3 H    = normalize(L + V);
    const f32 NdotV = max(dot(info.Normal, V), 1e-4);
    const f32 NdotH = max(dot(info.Normal, H), 0.0);
    const f32 VdotH = max(dot(V, H), 0.0);

    const f32 denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    const f32 D     = alpha2 / (PI * denom * denom);
    const f32 k     = alpha * 0.5;
    const f32 G     = (NdotV / (NdotV * (1.0 - k) + k)) *
                        (NdotL / (NdotL * (1.0 - k) + k));
    const f32v3 F    = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    const f32v3 specular   = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
    const f32v3 diffuse    = diffuseBase / PI;
    const f32v3 lightColor = unpackUnorm4x8ToFloat(color).xyz;
    return (diffuse * (1.f - F) + specular) * NdotL * lightColor;
}

f32m2x3 ComputeOrthonormalBase(const f32v3 ndir)
{
    const f32v3 up = abs(ndir.y) < 0.99 ? f32v3(0.f, 1.f, 0.f) : f32v3(1.f, 0.f, 0.f);
    const f32v3 t1 = normalize(cross(up, ndir));
    const f32v3 t2 = cross(ndir, t1);
    return f32m2x3(t1, t2);
}

// pbr is ai generated
f32v3 ComputeLightColor(const LightData3D data, const MaterialInfo3D info, const ResourceTable3D resources)
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

    const LightRange prg = GetRange(data.LightRanges[Light_Point]);
    for (u32 i = 0; i < prg.Count; ++i)
    {
        const PointLight3D plight = resources.PointLights[i + prg.Offset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v3 dir  = info.WorldPosition - f32v3(plight.PosX, plight.PosY, plight.PosZ);
        const f32 dist = length(dir);
        const f32v3 ndir = dir / dist;
        const f32v3 L    = -ndir;
        const f32v3 contrib = ComputeLightContribution(info, L, V, F0, diffuseBase, alpha, alpha2, plight.Color);

        const f32  radius     = plight.LightRadius * plight.LightRadius;
        const f32  dist2     = dist * dist;
        const f32  attenuation      = radius / (dist2 + radius);
        const f32 intensity = attenuation * plight.Intensity;

        f32 shadow = 1.f;
        if (bool(data.Flags & ShadedFlag_Shadows) && bool(plight.Flags & LightFlag_CastShadows))
        {
            const f32 z = min(1.f, dist / plight.ShadowRadius);
            const u32 shadowIndex = plight.ShadowMapOffset + countbits(plight.ViewMask & (data.ViewBit - 1));

            const TextureCube<f32> map = resources.PointMaps[shadowIndex];
            const SamplerComparisonState csmp = resources.ShadowCompareSampler;

            if (bool(plight.Flags & LightFlag_PCSS))
            {
                const SamplerState smp = resources.ShadowSampler;
                const f32 far = plight.ShadowRadius;
                const f32 near = ONYX_POINT_LIGHT_NEAR_RADIUS_FACTOR * far;

                const f32 tsize = data.TexelSizes[Light_Point];
                const f32 searchAngle = clamp((dist - near) * plight.LightSize / (near * dist), tsize, ONYX_MAX_SEARCH_RADIUS_TEXELS * tsize);

                const f32m2x3 orth = ComputeOrthonormalBase(ndir);
                const f32v3 t1 = orth[0];
                const f32v3 t2 = orth[1];

                u32 count = 0;
                f32 blockerDepth = 0.f;
                for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                {
                    const f32v2 disk = poissonDisk[i] * searchAngle;
                    const f32v3 offset = disk.x * t1 + disk.y * t2;
                    const f32 depth = map.Sample(smp, dir + offset);
                    if (depth < z)
                    {
                        ++count;
                        blockerDepth += depth;
                    }
                }
                if (count != 0)
                {
                    blockerDepth = blockerDepth * far / count;
                    const f32 penumbra = (dist - blockerDepth) * plight.LightSize / (blockerDepth * dist);
                    shadow = 0.f;
                    for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                    {
                        const f32v2 disk = poissonDisk[i] * penumbra;
                        const f32v3 offset = disk.x * t1 + disk.y * t2;
                        shadow += map.SampleCmp(csmp, dir + offset, z);
                    }
                    shadow /= POISSON_DISK_SAMPLES;
                }
            }
            else if (bool(plight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Point];
                const f32m2x3 orth = ComputeOrthonormalBase(ndir);
                const f32v3 t1 = orth[0];
                const f32v3 t2 = orth[1];
                shadow = 0.f;
                for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                {
                    const f32v2 disk = poissonDisk[i] * tsize;
                    const f32v3 offset = disk.x * t1 + disk.y * t2;
                    shadow += map.SampleCmp(csmp, dir + offset, z);
                }
                shadow /= POISSON_DISK_SAMPLES;
            }
            else
                shadow = map.SampleCmp(csmp, dir, z);
        }

        Lo += contrib * intensity * shadow;
    }

    const LightRange drg = GetRange(data.LightRanges[Light_Directional]);
    for (u32 i = 0; i < drg.Count; ++i)
    {
        const DirectionalLight3D dlight = resources.DirLights[i + drg.Offset];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const f32v3 dir = f32v3(dlight.DirX, dlight.DirY, dlight.DirZ);
        const f32v3 contrib = ComputeLightContribution(info, -dir, V, F0, diffuseBase, alpha, alpha2, dlight.Color);

        const f32v3 offset = f32v3(dlight.OffsetX, dlight.OffsetY, dlight.OffsetZ);
        const f32v2 extent = f32v2(dlight.ExtentX, dlight.ExtentY);

        const f32v3 t1 = f32v3(dlight.PlaneVec1X, dlight.PlaneVec1Y, dlight.PlaneVec1Z);
        const f32v3 t2 = f32v3(dlight.PlaneVec2X, dlight.PlaneVec2Y, dlight.PlaneVec2Z);

        const f32v3 diff = info.WorldPosition - offset;
        const f32v3 separation = diff - dot(diff, dir) * dir;
        const f32v2 localSep = f32v2(dot(separation, t1), dot(separation, t2));

        const f32 sep = length(localSep / extent);

        const f32 atten = f32(sep <= 1.f) * saturate(1.f - dlight.Decay * sep);
        const f32 intensity = dlight.Intensity * atten;

        f32 shadow = 1.f;
        if (bool(data.Flags & ShadedFlag_Shadows) && bool(dlight.Flags & LightFlag_CastShadows) && dlight.CascadeEnable != 0)
        {
            u32 cindex = dlight.CascadeCount - 1;
            for (u32 j = 0; j < dlight.CascadeCount; ++j)
                if (depth < dlight.Cascades[j].Split)
                {
                    cindex = j;
                    break;
                }

            if (bool((1U << cindex) & dlight.CascadeEnable))
            {
                const CascadeData cdata = dlight.Cascades[cindex];
                const u32 shadowIndex = dlight.ShadowMapOffset + countbits(dlight.ViewMask & (data.ViewBit - 1));
                const f32m4 pv = ConstructTransform(cdata.ProjectionView);

                const f32v3 projCoords = mul(f32v4(info.WorldPosition, 1.0), pv).xyz;
                const f32v2 shadowUV = projCoords.xy * 0.5 + 0.5;

                const Texture2DArray<f32> maps = resources.DirectionalMaps[shadowIndex];
                const SamplerComparisonState csmp = resources.ShadowCompareSampler;
                const f32 z = saturate(projCoords.z);

                if (bool(dlight.Flags & LightFlag_PCSS))
                {
                    const SamplerState smp = resources.ShadowSampler;
                    const f32 tsize = data.TexelSizes[Light_Directional];

                    const f32 tn = dlight.TanAngleSize;
                    const f32v2 txPerWorld = f32v2(cdata.ShadowInvSizeX, cdata.ShadowInvSizeY);
                    const f32v2 searchRadius = clamp(z * cdata.DepthRange * tn * txPerWorld, f32v2(tsize), f32v2(ONYX_MAX_SEARCH_RADIUS_TEXELS * tsize));

                    u32 count = 0;
                    f32 blockerDepth = 0.f;
                    for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                    {
                        const f32v2 offset = poissonDisk[i] * searchRadius;
                        const f32 depth = maps.Sample(smp, f32v3(shadowUV + offset, cindex));
                        if (depth < z)
                        {
                            ++count;
                            blockerDepth += depth;
                        }
                    }
                    if (count != 0)
                    {
                        blockerDepth /= count;
                        // const f32 penumbra = dlight.LightSize * (z - blockerDepth) / blockerDepth;
                        const f32v2 penumbra = tn * (z - blockerDepth) * cdata.DepthRange * txPerWorld;
                        shadow = 0.f;
                        for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                        {
                            const f32v2 offset = poissonDisk[i] * penumbra;
                            shadow += maps.SampleCmp(csmp, f32v3(shadowUV + offset, cindex), z);
                        }
                        shadow /= POISSON_DISK_SAMPLES;
                    }
                }
                else if (bool(dlight.Flags & LightFlag_PCF))
                {
                    const f32 tsize = data.TexelSizes[Light_Directional];
                    shadow = 0.f;
                    for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                    {
                        const f32v2 offset = poissonDisk[i] * tsize;
                        shadow += maps.SampleCmp(csmp, f32v3(shadowUV + offset, cindex), z);
                    }
                    shadow /= POISSON_DISK_SAMPLES;
                }
                else
                    shadow = maps.SampleCmp(csmp, f32v3(shadowUV, cindex), z);
            }
        }
        Lo += contrib * intensity * shadow;
    }

    const LightRange srg = GetRange(data.LightRanges[Light_Spot]);
    for (u32 i = 0; i < srg.Count; ++i)
    {
        const SpotLight slight = resources.SpLights[i + srg.Offset];
        if (!bool(slight.ViewMask & data.ViewBit))
            continue;

        const f32v3 dir  = info.WorldPosition - f32v3(slight.PosX, slight.PosY, slight.PosZ);
        const f32v3 lightDir = f32v3(slight.DirX, slight.DirY, slight.DirZ);

        const f32 dist = length(dir);
        const f32v3 L    = -dir / dist;
        const f32v3 contrib = ComputeLightContribution(info, L, V, F0, diffuseBase, alpha, alpha2, slight.Color);

        const f32 alignment = 0.5f * (1.f + dot(dir / dist, lightDir));
        const f32 thres = 0.5f * (1.f + slight.CosHalfPov);
        const f32 atten = f32(alignment >= thres) * saturate(1.f - slight.Decay * (1.f - alignment) / (1.f - thres));

        const f32  range     = slight.LightRange * slight.LightRange;
        const f32  dist2     = dist * dist;
        const f32  attenuation      = atten * atten * range / (dist2 + range);
        const f32 intensity = attenuation * slight.Intensity;

        f32 shadow = 1.f;
        if (bool(data.Flags & ShadedFlag_Shadows) && bool(slight.Flags & LightFlag_CastShadows))
        {
            const u32 shadowIndex = slight.ShadowMapOffset + countbits(slight.ViewMask & (data.ViewBit - 1));
            const f32m4 pv = ConstructPerspectiveTransform(slight.ProjectionView);

            const f32v4 lightSpacePos = mul(f32v4(info.WorldPosition, 1.f), pv);
            const f32v3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

            const f32v2 shadowUV = projCoords.xy * 0.5 + 0.5;
            const Texture2D<f32> map = resources.SpotMaps[shadowIndex];
            const SamplerComparisonState csmp = resources.ShadowCompareSampler;
            const f32 z = saturate(projCoords.z);

            if (bool(slight.Flags & LightFlag_PCSS))
            {
                const SamplerState smp = resources.ShadowSampler;
                const f32 tsize = data.TexelSizes[Light_Spot];

                const f32 n = slight.Near;
                const f32 f = slight.Far;
                const f32 zv = f * n / (f - z * (f - n));

                const f32v2 txPerWorld = f32v2(slight.ShadowInvSizeX, slight.ShadowInvSizeY) / zv;
                const f32v2 searchRadius = clamp(slight.LightSize * txPerWorld * (zv - n) / n, f32v2(tsize), f32v2(ONYX_MAX_SEARCH_RADIUS_TEXELS * tsize));

                u32 count = 0;
                f32 blockerDepth = 0.f;
                for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                {
                    const f32v2 offset = poissonDisk[i] * searchRadius;
                    const f32 depth = map.Sample(smp, shadowUV + offset);
                    if (depth < z)
                    {
                        ++count;
                        blockerDepth += f * n / (f - depth * (f - n));
                    }
                }
                if (count != 0)
                {
                    blockerDepth /= count;
                    const f32v2 penumbra = slight.LightSize * txPerWorld * (zv - blockerDepth) / blockerDepth;

                    shadow = 0.f;
                    for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                    {
                        const f32v2 offset = poissonDisk[i] * penumbra;
                        shadow += map.SampleCmp(csmp, shadowUV + offset, z);
                    }
                    shadow /= POISSON_DISK_SAMPLES;
                }
            }
            else if (bool(slight.Flags & LightFlag_PCF))
            {
                const f32 tsize = data.TexelSizes[Light_Spot];
                shadow = 0.f;
                for (u32 i = 0; i < POISSON_DISK_SAMPLES; ++i)
                {
                    const f32v2 offset = poissonDisk[i] * tsize;
                    shadow += map.SampleCmp(csmp, shadowUV + offset, z);
                }
                shadow /= POISSON_DISK_SAMPLES;
            }
            else
                shadow = map.SampleCmp(csmp, shadowUV, z);
        }
        Lo += contrib * intensity * shadow;
    }

    const f32v3 ambient = ambientColor.xyz * ambientColor.w * diffuseBase * info.Occlusion;

    return min(f32v3(1.0), ambient + Lo + info.Emissive);
}

f32v2 ComputeTexCoord(const f32v2 uv0, const f32 offset[2], const f32 scale[2])
{
    const f32v2 uvOffset = f32v2(offset[0], offset[1]);
    const f32v2 uvScale = f32v2(scale[0], scale[1]);
    return uvOffset + uv0 * uvScale;
}

f32v2 ComputeTexCoord(const f32v2 uv0, const f32 offset0[2], const f32 scale0[2], const u32 offset1, const u32 scale1)
{
    const f32v2 uvOffset = unpackHalf2x16ToFloat(offset1);
    const f32v2 uvScale = unpackHalf2x16ToFloat(scale1);

    return ComputeTexCoord(uvOffset + uv0 * uvScale, offset0, scale0);
}

f32v4 ComputeMaterialColor(const f32v4 fragColor, const LightData2D light, const u32 matId, const f32v2 worldPos, const f32v2 uv0, const ResourceTable2D resources, const f32 alpha, const u32 uvOffset, const u32 uvScale)
{
    if (matId == NullResource)
        return f32v4(fragColor.rgb, fragColor.a * alpha);

    const f32v3 lcolor = ComputeLightColor(light, worldPos, resources);

    const Material2D mat = resources.Materials[matId];
    const u32 sampId = GetResourceId(mat.Sampler);
    const u32 texId = GetResourceId(mat.Texture);

    const f32v2 uv = ComputeTexCoord(uv0, mat.TexOffset, mat.TexScale, uvOffset, uvScale);

    if (sampId == NullResource || texId == NullResource)
        return fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);

    const f32v4 tex = resources.Textures[texId].Sample(resources.Samplers[sampId], uv);
    return tex * fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);
}

f32v4 ComputeMaterialColor(const f32v4 fragColor, const LightData3D light, const u32 matId, const f32v3 worldPos, const f32v2 uv0, const ResourceTable3D resources, const f32 alpha, const f32m3 TBN, const bool isFrontFacing, const u32 uvOffset, const u32 uvScale)
{
    if (matId == NullResource)
        return f32v4(fragColor.rgb, fragColor.a * alpha);

    const Material3D mat = resources.Materials[matId];

    const u32 albedoSamp = GetResourceId(mat.Samplers[TextureSlot_Albedo]);
    const u32 normalSamp = GetResourceId(mat.Samplers[TextureSlot_Normal]);
    const u32 mrSamp = GetResourceId(mat.Samplers[TextureSlot_MetallicRoughness]);
    const u32 occSamp = GetResourceId(mat.Samplers[TextureSlot_Occlusion]);
    const u32 emissiveSamp = GetResourceId(mat.Samplers[TextureSlot_Emissive]);

    const u32 albedoTex = GetResourceId(mat.Textures[TextureSlot_Albedo]);
    const u32 normalTex = GetResourceId(mat.Textures[TextureSlot_Normal]);
    const u32 mrTex = GetResourceId(mat.Textures[TextureSlot_MetallicRoughness]);
    const u32 occTex = GetResourceId(mat.Textures[TextureSlot_Occlusion]);
    const u32 emissiveTex = GetResourceId(mat.Textures[TextureSlot_Emissive]);

    MaterialInfo3D info;
    info.Albedo = unpackUnorm4x8ToFloat(mat.AlbedoFactor);
    info.WorldPosition = worldPos;
    info.Emissive = f32v3(mat.EmissiveFactor[0], mat.EmissiveFactor[1], mat.EmissiveFactor[2]);
    info.Roughness = mat.RoughnessFactor;
    info.Metallic = mat.MetallicFactor;
    info.Occlusion = 1.0;

    const f32v2 uv = ComputeTexCoord(uv0, mat.TexOffset, mat.TexScale, uvOffset, uvScale);

    // Albedo
    if (albedoSamp != NullResource && albedoTex != NullResource)
        info.Albedo *= resources.Textures[albedoTex].Sample(resources.Samplers[albedoSamp], uv);

    // Normal map
    if (normalSamp != NullResource && normalTex != NullResource)
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
    if (mrSamp != NullResource && mrTex != NullResource)
    {
        const f32v4 mr = resources.Textures[mrTex].Sample(resources.Samplers[mrSamp], uv);
        info.Roughness *= mr.g;
        info.Metallic *= mr.b;
    }

    // Occlusion (R channel)
    if (occSamp != NullResource && occTex != NullResource)
    {
        const f32 sampledOcclusion = resources.Textures[occTex].Sample(resources.Samplers[occSamp], uv).r;
        info.Occlusion = 1.0 + mat.OcclusionStrength * (sampledOcclusion - 1.0);
    }

    // Emissive
    if (emissiveSamp != NullResource && emissiveTex != NullResource)
        info.Emissive *= resources.Textures[emissiveTex].Sample(resources.Samplers[emissiveSamp], uv).rgb;

    const f32v3 lcolor = ComputeLightColor(light, info, resources);
    return fragColor * f32v4(lcolor, info.Albedo.w * alpha);
}
