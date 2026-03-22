#pragma once

#include "alias.slang"

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
    u32 MatHandle;
}
struct StaticInstanceDataStencil2D
{
    Basis2D Basis;
    u32 Color;
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

[UnscopedEnum]
enum StadiumRegion : u32
{
    StadiumRegion_Body,
    StadiumRegion_Boundary,
    StadiumRegion_Moon,
}

struct InstanceParameters
{
    f32 Payload[2];
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

u32 GetMaterialIndex(const u32 handle)
{
    return handle & 0x00FFFFFF;
}
u32 GetMaterialPoolIndex(const u32 handle)
{
    return u32(handle >> 24);
}

f32m4 ComputeTransform2D(const Basis2D basis)
{
    return f32m4(
        f32v4(basis.Basis00, basis.Basis01, 0.0, 0.0),
        f32v4(basis.Basis10, basis.Basis11, 0.0, 0.0),
        f32v4(0.0,           0.0,           1.0, 0.0),
        f32v4(basis.Basis30, basis.Basis31, 0.0, 1.0)
    );
}

f32m4 ComputeTransform3D(const Basis3D basis)
{
    return f32m4(
        f32v4(basis.Basis00, basis.Basis01, basis.Basis02, 0.0),
        f32v4(basis.Basis10, basis.Basis11, basis.Basis12, 0.0),
        f32v4(basis.Basis20, basis.Basis21, basis.Basis22, 0.0),
        f32v4(basis.Basis30, basis.Basis31, basis.Basis32, 1.0)
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

// i admit lol ia generated
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
