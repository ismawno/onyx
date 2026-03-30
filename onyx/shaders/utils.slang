#pragma once

#include "qol.slang"

struct Basis2D
{
    f32 Basis00, Basis01;
    f32 Basis10, Basis11;
    f32 Basis30, Basis31;
}

struct Basis3D
{
    f32 Basis00, Basis10, Basis20, Basis30;
    f32 Basis01, Basis11, Basis21, Basis31;
    f32 Basis02, Basis12, Basis22, Basis32;
}

struct StaticInstanceDataFill2D
{
    Basis2D Basis;
    u32 Color;
    u32 DepthCounter;
    u32 MatHandle;
}
struct StaticInstanceDataStencil2D
{
    Basis2D Basis;
    u32 Color;
    u32 DepthCounter;
    f32 OutlineWidth;
}
struct StaticInstanceDataFill3D
{
    Basis3D Basis;
    u32 Color;
    u32 MatHandle;
}
struct StaticInstanceDataStencil3D
{
    Basis3D Basis;
    u32 Color;
    f32 OutlineWidth;
}

struct ArcData
{
    f32v4 CosSin;
    u32 AngleOverflow;
    f32 Hollowness;
}

struct ArcDataPacked
{
    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;
    u32 AngleOverflow;
    f32 Hollowness;
}

struct FadeData
{
    f32 InnerFade;
    f32 OuterFade;
}

struct CircleInstanceDataFill2D
{
    StaticInstanceDataFill2D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}
struct CircleInstanceDataStencil2D
{
    StaticInstanceDataStencil2D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}
struct CircleInstanceDataFill3D
{
    StaticInstanceDataFill3D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}
struct CircleInstanceDataStencil3D
{
    StaticInstanceDataStencil3D Data;
    ArcDataPacked Arc;
    FadeData Fade;
}

[UnscopedEnum]
enum ParametricShape : u32
{
    ParametricShape_Stadium,
    ParametricShape_RoundedQuad,
    ParametricShape_Capsule,
    ParametricShape_RoundedBox,
    ParametricShape_Torus,
    ParametricShape_Ring,
};

typedef u32 ParametricRegionFlags;

[UnscopedEnum]
enum StadiumRegion : ParametricRegionFlags
{
    StadiumRegion_Edge = 1 << 0,
    StadiumRegion_Moon = 1 << 1,
};

[UnscopedEnum]
enum RoundedQuadRegion : ParametricRegionFlags
{
    RoundedQuadRegion_HorizontalEdge = 1 << 0,
    RoundedQuadRegion_VerticalEdge = 1 << 1,
    RoundedQuadRegion_Moon = 1 << 2,
};

[UnscopedEnum]
enum CapsuleRegion : ParametricRegionFlags
{
    CapsuleRegion_Body = 1 << 0,
};

struct InstanceParameters
{
    f32 Payload[4];
    ParametricShape Shape;
}

struct ParametricInstanceDataFill2D
{
    StaticInstanceDataFill2D Data;
    InstanceParameters Parameters;
}
struct ParametricInstanceDataFill3D
{
    StaticInstanceDataFill3D Data;
    InstanceParameters Parameters;
}
struct ParametricInstanceDataStencil2D
{
    StaticInstanceDataStencil2D Data;
    InstanceParameters Parameters;
}
struct ParametricInstanceDataStencil3D
{
    StaticInstanceDataStencil3D Data;
    InstanceParameters Parameters;
}

struct GlyphInstanceDataFill2D
{
    StaticInstanceDataFill2D Data;
    u32 AtlasHandle;
    u32 SamplerHandle;
}
struct GlyphInstanceDataFill3D
{
    StaticInstanceDataFill3D Data;
    u32 AtlasHandle;
    u32 SamplerHandle;
}
struct GlyphInstanceDataStencil2D
{
    StaticInstanceDataStencil2D Data;
    u32 AtlasHandle;
    u32 SamplerHandle;
}
struct GlyphInstanceDataStencil3D
{
    StaticInstanceDataStencil3D Data;
    u32 AtlasHandle;
    u32 SamplerHandle;
}

struct Material2D
{
    u32 ColorFactor;
    u32 Sampler;
    u32 Texture;
}

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

    u32 AlbedoSamp;
    u32 MetallicRoughnessSamp;
    u32 NormalSamp;
    u32 OcclusionSamp;
    u32 EmissiveSamp;

    u32 AlbedoTex;
    u32 MetallicRoughnessTex;
    u32 NormalTex;
    u32 OcclusionTex;
    u32 EmissiveTex;
}

u32 GetAssetId(const u32 handle)
{
    return handle & ONYX_ASSET_ID_MASK;
}
u32 GetAssetPoolId(const u32 handle)
{
    return (handle & ONYX_ASSET_POOL_ID_MASK) >> ONYX_ASSET_POOL_SHIFT;
}

f32m4 ComputeTransform2D(const Basis2D basis)
{
    return f32m4(
        f32v4(basis.Basis00, basis.Basis01, 0.f, 0.f),
        f32v4(basis.Basis10, basis.Basis11, 0.f, 0.f),
        f32v4(0.f,           0.f,           0.f, 0.f),
        f32v4(basis.Basis30, basis.Basis31, 0.f, 1.f)
    );
}

f32 ComputeDepth(const u32 depthCounter)
{
    return 1.f - f32(depthCounter) / f32(u16(-1));
}

f32m4 ComputeTransform3D(const Basis3D basis)
{
    return f32m4(
        f32v4(basis.Basis00, basis.Basis01, basis.Basis02, 0.f),
        f32v4(basis.Basis10, basis.Basis11, basis.Basis12, 0.f),
        f32v4(basis.Basis20, basis.Basis21, basis.Basis22, 0.f),
        f32v4(basis.Basis30, basis.Basis31, basis.Basis32, 1.f)
    );
}
f32v2 ComputeStencilScale2D(const f32m4 transform)
{
    return f32v2(length(f32v2(transform[0][0], transform[1][0])), length(f32v2(transform[0][1], transform[1][1])));
}
f32v3 ComputeStencilScale3D(const f32m4 transform)
{
    return f32v3(length(f32v3(transform[0][0], transform[1][0], transform[2][0])), length(f32v3(transform[0][1], transform[1][1], transform[2][1])), length(f32v3(transform[0][2], transform[1][2], transform[2][2])));
}

f32m3 ComputeNormalMatrix(const f32m3 matrix)
{
    const f32v3 c0 = matrix[0];
    const f32v3 c1 = matrix[1];
    const f32v3 c2 = matrix[2];

    const f32v3 r0 = cross(c1, c2);
    const f32v3 r1 = cross(c2, c0);
    const f32v3 r2 = cross(c0, c1);

    const f32 det = dot(c0, r0);
    const f32 idet = sign(det) / max(abs(det), 1e-12);

    return f32m3(r0, r1, r2) * idet;
}

struct CircleBounds
{
    f32 Hollowness;
    f32 Radius;
}

CircleBounds GetCircleBounds(const f32v2 localPosition, const f32 hollowness)
{
    CircleBounds bounds;
    bounds.Hollowness = hollowness * hollowness;
    bounds.Radius = 4.0 * dot(localPosition, localPosition);
    return bounds;
}

bool CheckOutsideCircle(const CircleBounds bounds, const f32v2 localPosition, const ArcData arc)
{
    if (bounds.Radius > 1.f || bounds.Radius < bounds.Hollowness)
        return false;

    const f32 crs1 = localPosition.x * arc.CosSin[1] - localPosition.y * arc.CosSin[0];
    const f32 crs2 = localPosition.x * arc.CosSin[3] - localPosition.y * arc.CosSin[2];

    if (arc.AngleOverflow == 1)
    {
        if (crs1 > 0.f && crs2 < 0.f)
            return false;
    }
    else
    {
        if (crs1 > 0.f || crs2 < 0.f)
            return false;
    }
    return true;
}

f32 CircleFade(f32 alpha, const CircleBounds bounds, const f32v2 fade)
{
    const f32 innerFade = fade[0] * fade[0];
    const f32 outerFade = 1.0 - fade[1] * fade[1];
    const f32 shiftLen = (bounds.Radius - bounds.Hollowness) / (1.0 - bounds.Hollowness);

    if (shiftLen < innerFade)
        alpha *= shiftLen / innerFade;
    if (shiftLen > outerFade)
        alpha *= 1.0 - (shiftLen - outerFade) / (1.0 - outerFade);

    return clamp(alpha, 0.0, 1.0);
}

f32v2 GetParametricPosition2D(f32v2 pos, const InstanceParameters params, const ParametricRegionFlags region)
{
    if (params.Shape == ParametricShape_Stadium)
    {
        const f32v2 dims = f32v2(2.f * params.Payload[1], params.Payload[0]);
        const f32v2 s = sign(pos);
        pos += 0.5f * (dims - 1.f) * s;
        if (bool(region & StadiumRegion_Edge))
            pos[1] += (params.Payload[1] - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_RoundedQuad)
    {
        const f32v2 dims = f32v2(params.Payload[0], params.Payload[1]);
        const f32 radius = params.Payload[2];
        const f32v2 s = sign(pos);
        pos += 0.5f * (dims - 1.f) * s;
        if (bool(region & RoundedQuadRegion_HorizontalEdge))
            pos[0] += (radius - 0.5f) * s[0];
        if (bool(region & RoundedQuadRegion_VerticalEdge))
            pos[1] += (radius - 0.5f) * s[1];
    }
    return pos;
}

f32v3 GetParametricPosition3D(f32v3 pos, const f32v3 normal, const InstanceParameters params, const ParametricRegionFlags region)
{
    if (params.Shape == ParametricShape_Stadium)
    {
        const f32v2 dims = f32v2(2.f * params.Payload[1], params.Payload[0]);
        const f32v2 s = sign(pos.xy);
        pos.xy += 0.5f * (dims - 1.f) * s;
        if (bool(region & StadiumRegion_Edge))
            pos[1] += (params.Payload[1] - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_RoundedQuad)
    {
        const f32v2 dims = f32v2(params.Payload[0], params.Payload[1]);
        const f32 radius = params.Payload[2];
        const f32v2 s = sign(pos.xy);
        pos.xy += 0.5f * (dims - 1.f) * s;
        if (bool(region & RoundedQuadRegion_HorizontalEdge))
            pos[0] += (radius - 0.5f) * s[0];
        if (bool(region & RoundedQuadRegion_VerticalEdge))
            pos[1] += (radius - 0.5f) * s[1];
    }
    else if (params.Shape == ParametricShape_Capsule)
    {
        const f32 radius = params.Payload[1];
        const f32 diam = 2.f * radius;

        const f32v3 dims = f32v3(diam, params.Payload[0], diam);
        if (bool(region & CapsuleRegion_Body))
            pos *= dims;
        else
        {
            f32v3 dir = pos;
            const f32 s = sign(dir.y);
            dir.y -= 0.5f * s;
            pos += (radius - 0.5f) * normalize(dir);
            pos[1] += 0.5f * (dims[1] - 1.f) * s;
        }
    }
    else if (params.Shape == ParametricShape_RoundedBox)
    {
        const f32v3 dims = f32v3(params.Payload[0], params.Payload[1], params.Payload[2]);
        const f32 radius = params.Payload[3];
        pos += 0.5f * (dims - 1.f) * sign(pos);
        pos += (radius - 0.5f) * normal;
    }
    else if (params.Shape == ParametricShape_Torus)
    {
        const f32 iradius = params.Payload[0];
        const f32 oradius = params.Payload[1];

        const f32v2 dir = pos.xz;

        pos.xz += (oradius - 0.5f) * normalize(dir);
        pos += (iradius - 0.5f) * normal;
    }
    return pos;
}

bool CheckParametricFragment2D(f32v2 pos, const u32v2 shapeInfo)
{
    const u32 shape = shapeInfo[0];
    const u32 region = shapeInfo[1];
    if (shape == ParametricShape_Stadium && bool(region & StadiumRegion_Moon))
    {
        pos.y -= 0.5f * sign(pos.y);
        const f32 len = 4.f * dot(pos, pos);
        if (len > 1.f)
            return false;
    }
    else if (shape == ParametricShape_RoundedQuad && bool(region & RoundedQuadRegion_Moon))
    {
        pos -= 0.5f * sign(pos);
        const f32 len = 4.f * dot(pos, pos);
        if (len > 1.f)
            return false;
    }

    return true;
}

bool CheckParametricFragment3D(f32v3 pos, const u32v2 shapeInfo)
{
    const u32 shape = shapeInfo[0];
    const u32 region = shapeInfo[1];
    if (shape == ParametricShape_Stadium && bool(region & StadiumRegion_Moon))
    {
        f32v2 p = pos.xy;
        p.y -= 0.5f * sign(p.y);
        const f32 len = 4.f * dot(p, p);
        if (len > 1.f)
            return false;
    }
    else if (shape == ParametricShape_RoundedQuad && bool(region & RoundedQuadRegion_Moon))
    {
        f32v2 p = pos.xy;
        p -= 0.5f * sign(p);
        const f32 len = 4.f * dot(p, p);
        if (len > 1.f)
            return false;
    }
    return true;
}

struct LightData2D
{
    u32 PointLightOffset;
    u32 PointLightCount;
    u32 AmbientColor;
    ViewMask ViewBit;
}

struct LightData3D
{
    f32v4 ViewPosition;
    u32 PointLightOffset;
    u32 PointLightCount;
    u32 DirectionalLightOffset;
    u32 DirectionalLightCount;
    u32 AmbientColor;
    ViewMask ViewBit;
}

struct PointLight2D
{
    f32 PosX;
    f32 PosY;
    f32 Intensity;
    f32 Radius;
    u32 Color;
    ViewMask ViewMask;
}

struct PointLight3D
{
    f32 PosX;
    f32 PosY;
    f32 PosZ;
    f32 Intensity;
    f32 Radius;
    u32 Color;
    ViewMask ViewMask;
}

struct AmbientLight
{
    u32 Color;
    ViewMask ViewMask;
}

struct DirectionalLight
{
    f32 DirX;
    f32 DirY;
    f32 DirZ;
    f32 Intensity;
    u32 Color;
    ViewMask ViewMask;
}

typedef StructuredBuffer<PointLight2D, Std430DataLayout> PointLights2D;
typedef StructuredBuffer<PointLight3D, Std430DataLayout> PointLights3D;

typedef StructuredBuffer<Material2D, Std430DataLayout> Materials2D;
typedef StructuredBuffer<Material3D, Std430DataLayout> Materials3D;

typedef StructuredBuffer<DirectionalLight, Std430DataLayout> DirectionalLights;

f32v3 GetLightColor2D(const LightData2D data, const f32v2 worldPosition, const PointLights2D plights)
{
    f32v3 diffuseColor = f32v3(0.0);
    const f32v4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    for (u32 i = 0; i < data.PointLightCount; ++i)
    {
        const PointLight2D plight = plights[i + data.PointLightOffset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v2 direction = f32v2(plight.PosX, plight.PosY) - worldPosition;
        const f32 dist = dot(direction, direction);
        const f32v3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;
        const f32 radius = plight.Radius * plight.Radius;
        const f32 attenuation = radius / (dist + radius);
        const f32 intensity = attenuation * plight.Intensity;
        diffuseColor += intensity * lightColor;
    }

    return ambientColor.xyz * ambientColor.w + diffuseColor;
}

struct ResourceTable2D
{
    Materials2D Materials[ONYX_MAX_ASSET_POOLS];
    SamplerState Samplers[ONYX_MAX_SAMPLERS];
    Texture2D Textures[ONYX_MAX_TEXTURES];
    PointLights2D PointLights;
}

f32v4 ComputeColor2D(const f32v4 fragColor, const LightData2D light, const u32 matHandle, const f32v2 worldPos, const f32v2 uv, const ResourceTable2D resources, const f32 alpha)
{
    const u32 matId = GetAssetId(matHandle);
    if (matId != NullAsset)
    {
        const f32v3 lcolor = GetLightColor2D(light, worldPos, resources.PointLights);
        const u32 poolId = GetAssetPoolId(matHandle);

        const Material2D mat = resources.Materials[poolId][matId];

        const u32 sampId = GetAssetId(mat.Sampler);
        const u32 texId = GetAssetId(mat.Texture);

        if (sampId == NullAsset || texId == NullAsset)
            return fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);

        const f32v4 tex = resources.Textures[texId].Sample(resources.Samplers[matId], uv);
        return tex * fragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);
    }
    return f32v4(fragColor.rgb, fragColor.a * alpha);
}

f32 ComputeMTSDFCoverage(const SamplerState samplers[ONYX_MAX_SAMPLERS], const Texture2D textures[ONYX_MAX_TEXTURES], const u32 samplerId, const u32 atlasId, const f32v2 uv)
{
    // Sample the MTSDF atlas
    const f32v4 mtsdf = textures[atlasId].Sample(samplers[samplerId], uv);

    // Median of RGB channels for MTSDF
    const f32 median = max(min(mtsdf.r, mtsdf.g), min(max(mtsdf.r, mtsdf.g), mtsdf.b));

    // Screen-space derivative for adaptive smoothing
    const f32 smoothing = fwidth(median) * 0.5;
    const f32 coverage = smoothstep(0.5 - smoothing, 0.5 + smoothing, median);

    // Also use the alpha channel (MTSDF has true SDF in alpha)
    const f32 alphaCoverage = smoothstep(0.5 - smoothing, 0.5 + smoothing, mtsdf.a);
    return min(coverage, alphaCoverage);
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

// this is ia generated. im not familiar with lighting techniques
f32v3 GetLightColor3D(const LightData3D data, const MaterialInfo3D info)
{
    const f32v4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    const f32v3 V = normalize(data.ViewPosition.xyz - info.WorldPosition);

    const f32v3 baseColor = info.Albedo.xyz;
    const f32 metallic   = info.Metallic;
    const f32 roughness  = info.Roughness;
    const f32 alpha      = roughness * roughness;
    const f32 alpha2     = alpha * alpha;

    const f32v3 F0          = lerp(f32v3(0.04), baseColor, metallic);
    const f32v3 diffuseBase = baseColor * (1.0 - metallic);

    f32v3 Lo = f32v3(0.0);

    for (u32 i = 0; i < data.DirectionalLightCount; ++i)
    {
        const DirectionalLight dlight = info.Directionals[i + data.DirectionalLightOffset];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const f32v3 L    = f32v3(dlight.DirX, dlight.DirY, dlight.DirZ);
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
        const f32v3 lightColor = unpackUnorm4x8ToFloat(dlight.Color).xyz;

        Lo += (diffuse * (1.0 - F) + specular) * lightColor * dlight.Intensity * NdotL;
    }

    for (u32 i = 0; i < data.PointLightCount; ++i)
    {
        const PointLight3D plight = info.Points[i + data.PointLightOffset];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const f32v3 dir  = f32v3(plight.PosX, plight.PosY, plight.PosZ) - info.WorldPosition;
        const f32v3 L    = normalize(dir);
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
        const f32  radius     = plight.Radius * plight.Radius;
        const f32  dirLen     = dot(dir, dir);
        const f32  atten      = radius / (dirLen + radius);
        const f32v3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;

        Lo += (diffuse * (1.0 - F) + specular) * lightColor * atten * plight.Intensity * NdotL;
    }

    const f32v3 ambient = ambientColor.xyz * ambientColor.w * diffuseBase * info.Occlusion;

    return min(f32v3(1.0), ambient + Lo + info.Emissive);
}

struct ResourceTable3D
{
    Materials3D Materials[ONYX_MAX_ASSET_POOLS];
    SamplerState Samplers[ONYX_MAX_SAMPLERS];
    Texture2D Textures[ONYX_MAX_TEXTURES];
    PointLights3D PointLights;
    DirectionalLights DirLights;
}

f32v4 ComputeColor3D(const f32v4 fragColor, const LightData3D light, const u32 matHandle, const f32v3 worldPos, const f32v2 uv, const ResourceTable3D resources, const f32 alpha, const f32m3 TBN, const bool isFrontFacing)
{
    const u32 matId = GetAssetId(matHandle);
    if (matId != NullAsset)
    {
        const u32 poolId = GetAssetPoolId(matHandle);
        const Material3D mat = resources.Materials[poolId][matId];

        const u32 albedoSamp = GetAssetId(mat.AlbedoSamp);
        const u32 albedoTex = GetAssetId(mat.AlbedoTex);

        const u32 normalSamp = GetAssetId(mat.NormalSamp);
        const u32 normalTex = GetAssetId(mat.NormalTex);

        const u32 mrSamp = GetAssetId(mat.MetallicRoughnessSamp);
        const u32 mrTex = GetAssetId(mat.MetallicRoughnessTex);

        const u32 occSamp = GetAssetId(mat.OcclusionSamp);
        const u32 occTex = GetAssetId(mat.OcclusionTex);

        const u32 emissiveSamp = GetAssetId(mat.EmissiveSamp);
        const u32 emissiveTex = GetAssetId(mat.EmissiveTex);

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
            info.Normal = normalize(mul(sampledNormal, TBN)); // requires TBN matrix in VertexOutput
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

        const f32v3 lcolor = GetLightColor3D(light, info);
        return fragColor * f32v4(lcolor, info.Albedo.w * alpha);
    }

    return f32v4(fragColor.rgb, fragColor.a * alpha);
}
