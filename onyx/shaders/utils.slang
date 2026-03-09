#pragma once

public struct Basis2D
{
    float Basis00, Basis01;
    float Basis10, Basis11;
    float Basis30, Basis31;
};

public struct Basis3D
{
    float Basis00, Basis10, Basis20, Basis30;
    float Basis01, Basis11, Basis21, Basis31;
    float Basis02, Basis12, Basis22, Basis32;
};

public struct InstanceDataFill2D
{
    Basis2D Basis;
    uint Color;
    uint MatIndex;
};
public struct InstanceDataStencil2D
{
    Basis2D Basis;
    uint Color;
    float OutlineWidth;
};
public struct InstanceDataFill3D
{
    Basis3D Basis;
    uint Color;
    uint MatIndex;
};
public struct InstanceDataStencil3D
{
    Basis3D Basis;
    uint Color;
    float OutlineWidth;
};

public struct ArcData
{
    float4 CosSin;
    uint AngleOverflow;
    float Hollowness;
};

public struct ArcDataPacked
{
    float LowerCos;
    float LowerSin;
    float UpperCos;
    float UpperSin;
    uint AngleOverflow;
    float Hollowness;
};

public struct FadeData
{
    float InnerFade;
    float OuterFade;
};

public struct CircleInstanceDataFill2D
{
    InstanceDataFill2D Data;
    ArcDataPacked Arc;
    FadeData Fade;
};
public struct CircleInstanceDataStencil2D
{
    InstanceDataStencil2D Data;
    ArcDataPacked Arc;
    FadeData Fade;
};
public struct CircleInstanceDataFill3D
{
    InstanceDataFill3D Data;
    ArcDataPacked Arc;
    FadeData Fade;
};
public struct CircleInstanceDataStencil3D
{
    InstanceDataStencil3D Data;
    ArcDataPacked Arc;
    FadeData Fade;
};

public struct Material2D
{
    uint ColorFactor;
    uint Texture;
    uint Sampler;
}

public struct Material3D
{
    float EmissiveFactorX;
    float EmissiveFactorY;
    float EmissiveFactorZ;
    uint AlbedoFactor;
    float MetallicFactor;
    float RoughnessFactor;
    float OcclusionStrength;
    float NormalScale;
    uint Sampler;
    uint AlbedoTex;
    uint MetallicRoughnessTex;
    uint NormalTex;
    uint OcclusionTex;
    uint EmissiveTex;
}


float4x4 ComputeTransform2D(const Basis2D basis)
{
    return float4x4(
        float4(basis.Basis00, basis.Basis01, 0.0, 0.0),
        float4(basis.Basis10, basis.Basis11, 0.0, 0.0),
        float4(0.0,           0.0,           1.0, 0.0),
        float4(basis.Basis30, basis.Basis31, 0.0, 1.0)
    );
}

float4x4 ComputeTransform3D(const Basis3D basis)
{
    return float4x4(
        float4(basis.Basis00, basis.Basis01, basis.Basis02, 0.0),
        float4(basis.Basis10, basis.Basis11, basis.Basis12, 0.0),
        float4(basis.Basis20, basis.Basis21, basis.Basis22, 0.0),
        float4(basis.Basis30, basis.Basis31, basis.Basis32, 1.0)
    );
}
float2 ComputeStencilScale2D(const float4x4 transform)
{
    return float2(length(float2(transform[0][0], transform[1][0])), length(float2(transform[0][1], transform[1][1])));
}
float3 ComputeStencilScale3D(const float4x4 transform)
{
    return float3(length(float3(transform[0][0], transform[1][0], transform[2][0])), length(float3(transform[0][1], transform[1][1], transform[2][1])), length(float3(transform[0][2], transform[1][2], transform[2][2])));
}

float3x3 ComputeNormalMatrix(const float3x3 matrix)
{
    const float3 c0 = matrix[0];
    const float3 c1 = matrix[1];
    const float3 c2 = matrix[2];

    const float3 r0 = cross(c1, c2);
    const float3 r1 = cross(c2, c0);
    const float3 r2 = cross(c0, c1);

    const float det = dot(c0, r0);
    const float idet = sign(det) / max(abs(det), 1e-12);

    return float3x3(r0, r1, r2) * idet;
}

public struct CircleBounds
{
    float Hollowness;
    float Radius;
};

CircleBounds GetCircleBounds(const float2 localPosition, const float hollowness)
{
    CircleBounds bounds;
    bounds.Hollowness = hollowness * hollowness;
    bounds.Radius = 4.0 * dot(localPosition, localPosition);
    return bounds;
}

bool CheckOutsideCircle(const CircleBounds bounds, const float2 localPosition, const ArcData arc)
{
    if (bounds.Radius > 1.0 || bounds.Radius < bounds.Hollowness)
        return false;

    const float crs1 = localPosition.x * arc.CosSin[1] - localPosition.y * arc.CosSin[0];
    const float crs2 = localPosition.x * arc.CosSin[3] - localPosition.y * arc.CosSin[2];

    if (arc.AngleOverflow == 1)
    {
        if (crs1 > 0.0 && crs2 < 0.0)
            return false;
    }
    else
    {
        if (crs1 > 0.0 || crs2 < 0.0)
            return false;
    }
    return true;
}

float CircleFade(float alpha, const CircleBounds bounds, const float2 fade)
{
    const float innerFade = fade[0] * fade[0];
    const float outerFade = 1.0 - fade[1] * fade[1];
    const float shiftLen = (bounds.Radius - bounds.Hollowness) / (1.0 - bounds.Hollowness);

    if (shiftLen < innerFade)
        alpha *= shiftLen / innerFade;
    if (shiftLen > outerFade)
        alpha *= 1.0 - (shiftLen - outerFade) / (1.0 - outerFade);

    return clamp(alpha, 0.0, 1.0);
}

public struct LightData2D
{
    uint PointLightCount;
    uint AmbientColor;
    uint ViewBit;
};

public struct LightData3D
{
    float4 ViewPosition;
    uint PointLightCount;
    uint DirectionalLightCount;
    uint AmbientColor;
    uint ViewBit;
};

public struct PointLight2D
{
    float PosX;
    float PosY;
    float Intensity;
    float Radius;
    uint Color;
    uint ViewMask;
};

public struct PointLight3D
{
    float PosX;
    float PosY;
    float PosZ;
    float Intensity;
    float Radius;
    uint Color;
    uint ViewMask;
};

public struct AmbientLight
{
    uint Color;
    uint ViewMask;
};

public struct DirectionalLight
{
    float DirX;
    float DirY;
    float DirZ;
    float Intensity;
    uint Color;
    uint ViewMask;
};

typedef StructuredBuffer<PointLight2D, Std430DataLayout> PointLights2D;
typedef StructuredBuffer<PointLight3D, Std430DataLayout> PointLights3D;

typedef StructuredBuffer<Material2D, Std430DataLayout> Materials2D;
typedef StructuredBuffer<Material3D, Std430DataLayout> Materials3D;

typedef StructuredBuffer<DirectionalLight, Std430DataLayout> DirectionalLights;

float3 GetLightColor2D(const LightData2D data, const float2 worldPosition, const PointLights2D plights)
{
    float3 diffuseColor = float3(0.0);
    const float4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    for (uint i = 0; i < data.PointLightCount; ++i)
    {
        const PointLight2D plight = plights[i];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const float2 direction = float2(plight.PosX, plight.PosY) - worldPosition;
        const float dist = dot(direction, direction);
        const float3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;
        const float radius = plight.Radius * plight.Radius;
        const float attenuation = radius / (dist + radius);
        const float intensity = attenuation * plight.Intensity;
        diffuseColor += intensity * lightColor;
    }

    return ambientColor.xyz * ambientColor.w + diffuseColor;
}

struct MaterialInfo3D
{
    PointLights3D Points;
    DirectionalLights Directionals;
    float4 Albedo;
    float3 WorldPosition;
    float3 Normal;
    float3 Emissive;
    float Roughness;
    float Metallic;
    float Occlusion;
};

// i admit lol ia generated
float3 GetLightColor3D(const LightData3D data, const MaterialInfo3D info)
{
    const float4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    const float3 V = normalize(data.ViewPosition.xyz - info.WorldPosition);

    const float3 baseColor = info.Albedo.xyz;
    const float metallic   = info.Metallic;
    const float roughness  = info.Roughness;
    const float alpha      = roughness * roughness;
    const float alpha2     = alpha * alpha;

    const float3 F0          = lerp(float3(0.04), baseColor, metallic);
    const float3 diffuseBase = baseColor * (1.0 - metallic);

    float3 Lo = float3(0.0);

    for (uint i = 0; i < data.DirectionalLightCount; ++i)
    {
        const DirectionalLight dlight = info.Directionals[i];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const float3 L    = float3(dlight.DirX, dlight.DirY, dlight.DirZ);
        const float NdotL = dot(info.Normal, L);
        if (NdotL <= 0.0) continue;

        const float3 H    = normalize(L + V);
        const float NdotV = max(dot(info.Normal, V), 1e-4);
        const float NdotH = max(dot(info.Normal, H), 0.0);
        const float VdotH = max(dot(V, H), 0.0);

        const float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
        const float D     = alpha2 / (3.14159265 * denom * denom);
        const float k     = alpha * 0.5;
        const float G     = (NdotV / (NdotV * (1.0 - k) + k)) *
                            (NdotL / (NdotL * (1.0 - k) + k));
        const float3 F    = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        const float3 specular   = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        const float3 diffuse    = diffuseBase / 3.14159265;
        const float3 lightColor = unpackUnorm4x8ToFloat(dlight.Color).xyz;

        Lo += (diffuse * (1.0 - F) + specular) * lightColor * dlight.Intensity * NdotL;
    }

    for (uint i = 0; i < data.PointLightCount; ++i)
    {
        const PointLight3D plight = info.Points[i];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const float3 dir  = float3(plight.PosX, plight.PosY, plight.PosZ) - info.WorldPosition;
        const float3 L    = normalize(dir);
        const float NdotL = dot(info.Normal, L);
        if (NdotL <= 0.0) continue;

        const float3 H    = normalize(L + V);
        const float NdotV = max(dot(info.Normal, V), 1e-4);
        const float NdotH = max(dot(info.Normal, H), 0.0);
        const float VdotH = max(dot(V, H), 0.0);

        const float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
        const float D     = alpha2 / (3.14159265 * denom * denom);
        const float k     = alpha * 0.5;
        const float G     = (NdotV / (NdotV * (1.0 - k) + k)) *
                            (NdotL / (NdotL * (1.0 - k) + k));
        const float3 F    = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        const float3 specular   = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        const float3 diffuse    = diffuseBase / 3.14159265;
        const float  radius     = plight.Radius * plight.Radius;
        const float  dirLen     = dot(dir, dir);
        const float  atten      = radius / (dirLen + radius);
        const float3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;

        Lo += (diffuse * (1.0 - F) + specular) * lightColor * atten * plight.Intensity * NdotL;
    }

    const float3 ambient = ambientColor.xyz * ambientColor.w * diffuseBase * info.Occlusion;

    return min(float3(1.0), ambient + Lo + info.Emissive);
}
