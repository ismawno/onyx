#pragma once

public struct Basis2D
{
    float Basis11, Basis12;
    float Basis21, Basis22;
    float Basis31, Basis32;
};

public struct Basis3D
{
    float Basis11, Basis12, Basis13, Basis14;
    float Basis21, Basis22, Basis23, Basis24;
    float Basis31, Basis32, Basis33, Basis34;
};

public struct InstanceData2D
{
    Basis2D Basis;
    uint Color;
    uint TexIndex;
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

public struct CircleInstanceData2D
{
    InstanceData2D Data;
    ArcData Arc;
    FadeData Fade;
};
public struct CircleInstanceDataFill3D
{
    InstanceDataFill3D Data;
    ArcData Arc;
    FadeData Fade;
};
public struct CircleInstanceDataStencil3D
{
    InstanceDataStencil3D Data;
    ArcData Arc;
    FadeData Fade;
};

float4x4 ComputeTransform2D(const Basis2D basis)
{
    return float4x4(
        float4(basis.Basis11, basis.Basis21, 0.0, basis.Basis31),
        float4(basis.Basis12, basis.Basis22, 0.0, basis.Basis32),
        float4(0.0,               0.0,              1.0, 0.0),
        float4(0.0,               0.0,              0.0, 1.0)
    );
}

float4x4 ComputeTransform3D(const Basis3D basis)
{
    return float4x4(
        float4(basis.Basis11, basis.Basis12, basis.Basis13, basis.Basis14),
        float4(basis.Basis21, basis.Basis22, basis.Basis23, basis.Basis24),
        float4(basis.Basis31, basis.Basis32, basis.Basis33, basis.Basis34),
        float4(0.0,               0.0,               0.0,               1.0)
    );
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
float3 ComputeTrivialWorldNormal(const float3x3 matrix)
{
    const float3 c0 = matrix[0];
    const float3 c1 = matrix[1];
    const float3 c2 = matrix[2];

    const float3 r0 = cross(c1, c2);
    const float3 r2 = cross(c0, c1);

    const float det = dot(c0, r0);
    const float idet = sign(det) / max(abs(det), 1e-12);

    return r2 * idet;
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

    const float crs1 = localPosition.x * arc.LowerSin - localPosition.y * arc.LowerCos;
    const float crs2 = localPosition.x * arc.UpperSin - localPosition.y * arc.UpperCos;

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

float CircleFade(float alpha, const CircleBounds bounds, const FadeData fade)
{
    const float innerFade = fade.InnerFade * fade.InnerFade;
    const float outerFade = 1.0 - fade.OuterFade * fade.OuterFade;
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
    float PosX;
    float PosY;
    float PosZ;
    float Intensity;
    uint Color;
    uint ViewMask;
};

typedef StructuredBuffer<PointLight2D, Std430DataLayout> PointLights2D;
typedef StructuredBuffer<PointLight3D, Std430DataLayout> PointLights3D;

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

float3 GetLightColor3D(const LightData3D data, const float3 normal, const float3 worldPosition, const PointLights3D plights, const DirectionalLights dlights)
{
    const float DiffuseContribution = 0.8f;
    const float SpecularContribution = 0.2f;
    const float SpecularSharpness = 8;

    const float4 ambientColor = unpackUnorm4x8ToFloat(data.AmbientColor);
    const float3 specularDirection = normalize(data.ViewPosition.xyz - worldPosition);

    float3 diffuseColor = float3(0.0);
    float3 specularColor = float3(0.0);
    for (uint i = 0; i < data.DirectionalLightCount; ++i)
    {
        const DirectionalLight dlight = dlights[i];
        if (!bool(dlight.ViewMask & data.ViewBit))
            continue;

        const float3 direction = float3(dlight.PosY, dlight.PosY, dlight.PosZ);
        const float product = dot(normal, direction);

        if (product > 0.0)
        {
            const float intensity = dlight.Intensity;
            const float3 lightColor = unpackUnorm4x8ToFloat(dlight.Color).xyz;
            diffuseColor += intensity * lightColor * product;

            const float3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(normal, halfVector), 0.0), SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    for (uint i = 0; i < data.PointLightCount; ++i)
    {
        const PointLight3D plight = plights[i];
        if (!bool(plight.ViewMask & data.ViewBit))
            continue;

        const float3 direction = float3(plight.PosX, plight.PosY, plight.PosZ) - worldPosition;
        const float product = dot(normal, direction);

        if (product > 0.0)
        {
            const float3 lightColor = unpackUnorm4x8ToFloat(plight.Color).xyz;
            const float radius = plight.Radius * plight.Radius;
            const float dirLen = dot(direction, direction);
            const float attenuation = radius / (dirLen + radius);
            const float intensity = attenuation * plight.Intensity;
            diffuseColor += intensity * lightColor * dot(normal, normalize(direction));

            const float3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(normal, halfVector), 0.0), SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    return ambientColor.xyz * ambientColor.w +
        DiffuseContribution * diffuseColor + SpecularContribution * specularColor;
}

