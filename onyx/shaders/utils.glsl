#ifndef ONYX_UTILS_GLSL
#define ONYX_UTILS_GLSL

struct ReflectionData3D
{
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

struct Basis2D
{
    float Basis11, Basis12;
    float Basis21, Basis22;
    float Basis31, Basis32;
};

struct Basis3D
{
    float Basis11, Basis12, Basis13, Basis14;
    float Basis21, Basis22, Basis23, Basis24;
    float Basis31, Basis32, Basis33, Basis34;
};

struct InstanceData2D
{
    Basis2D Basis;
    uint Color;
};
struct InstanceDataFill3D
{
    Basis3D Basis;
    uint Color;
    ReflectionData3D Reflection;
};
struct InstanceDataStencil3D
{
    Basis3D Basis;
    uint Color;
};

struct ArcData
{
    float LowerCos;
    float LowerSin;
    float UpperCos;
    float UpperSin;
    uint AngleOverflow;
    float Hollowness;
};

struct FadeData
{
    float InnerFade;
    float OuterFade;
};

struct CircleInstanceData2D
{
    InstanceData2D Data;
    ArcData Arc;
    FadeData Fade;
};
struct CircleInstanceDataFill3D
{
    InstanceDataFill3D Data;
    ArcData Arc;
    FadeData Fade;
};
struct CircleInstanceDataStencil3D
{
    InstanceDataStencil3D Data;
    ArcData Arc;
    FadeData Fade;
};

struct MaterialData3D
{
    vec4 Color;
    ReflectionData3D Reflection;
};

mat4 ComputeTransform2D(const Basis2D p_Basis)
{
    return mat4(vec4(p_Basis.Basis11, p_Basis.Basis12, 0.0, 0.0), vec4(p_Basis.Basis21, p_Basis.Basis22, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(p_Basis.Basis31, p_Basis.Basis32, 0.0, 1.0));
}
mat4 ComputeTransform3D(const Basis3D p_Basis)
{
    return mat4(vec4(p_Basis.Basis11, p_Basis.Basis21, p_Basis.Basis31, 0.0), vec4(p_Basis.Basis12, p_Basis.Basis22, p_Basis.Basis32, 0.0), vec4(p_Basis.Basis13, p_Basis.Basis23, p_Basis.Basis33, 0.0), vec4(p_Basis.Basis14, p_Basis.Basis24, p_Basis.Basis34, 1.0));
}

mat3 ComputeNormalMatrix(const mat3 p_Matrix)
{
    const vec3 c0 = p_Matrix[0];
    const vec3 c1 = p_Matrix[1];
    const vec3 c2 = p_Matrix[2];

    const vec3 r0 = cross(c1, c2);
    const vec3 r1 = cross(c2, c0);
    const vec3 r2 = cross(c0, c1);

    const float det = dot(c0, r0);
    const float idet = sign(det) / max(abs(det), 1e-12);

    return mat3(r0, r1, r2) * idet;
}
vec3 ComputeTrivialWorldNormal(const mat3 p_Matrix)
{
    const vec3 c0 = p_Matrix[0];
    const vec3 c1 = p_Matrix[1];
    const vec3 c2 = p_Matrix[2];

    const vec3 r0 = cross(c1, c2);
    const vec3 r2 = cross(c0, c1);

    const float det = dot(c0, r0);
    const float idet = sign(det) / max(abs(det), 1e-12);

    return r2 * idet;
}

struct CircleBounds
{
    float Hollowness;
    float Radius;
};

CircleBounds GetCircleBounds(const vec2 p_LocalPosition, const float p_Hollowness)
{
    CircleBounds bounds;
    bounds.Hollowness = p_Hollowness * p_Hollowness;
    bounds.Radius = 4.0 * dot(p_LocalPosition, p_LocalPosition);
    return bounds;
}

bool CheckOutsideCircle(const CircleBounds p_Bounds, const vec2 p_LocalPosition, const ArcData p_Arc)
{
    if (p_Bounds.Radius > 1.0 || p_Bounds.Radius < p_Bounds.Hollowness)
        return false;

    const float crs1 = p_LocalPosition.x * p_Arc.LowerSin - p_LocalPosition.y * p_Arc.LowerCos;
    const float crs2 = p_LocalPosition.x * p_Arc.UpperSin - p_LocalPosition.y * p_Arc.UpperCos;

    if (p_Arc.AngleOverflow == 1)
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

float CircleFade(float p_Alpha, const CircleBounds p_Bounds, const FadeData p_Fade)
{
    const float innerFade = p_Fade.InnerFade * p_Fade.InnerFade;
    const float outerFade = 1.0 - p_Fade.OuterFade * p_Fade.OuterFade;
    const float shiftLen = (p_Bounds.Radius - p_Bounds.Hollowness) / (1.0 - p_Bounds.Hollowness);

    if (shiftLen < innerFade)
        p_Alpha *= shiftLen / innerFade;
    if (shiftLen > outerFade)
        p_Alpha *= 1.0 - (shiftLen - outerFade) / (1.0 - outerFade);

    return clamp(p_Alpha, 0.0, 1.0);
}

struct LightData3D
{
    vec4 ViewPosition;
    vec4 AmbientColor;
    uint DirectionalLightCount;
    uint PointLightCount;
};

#ifdef ONYX_LIGHTS

struct DirectionalLight
{
    float PosX;
    float PosY;
    float PosZ;
    float Intensity;
    uint Color;
};

struct PointLight
{
    float PosX;
    float PosY;
    float PosZ;
    float Intensity;
    float Radius;
    uint Color;
};

layout(std430, set = 1, binding = 0) readonly buffer DirectionalLights
{
    DirectionalLight Lights[];
}
directionalLights;

layout(std430, set = 1, binding = 1) readonly buffer PointLights
{
    PointLight Lights[];
}
pointLights;

vec3 GetLightColor(const LightData3D p_Data, const vec3 p_Normal, const ReflectionData3D p_Reflection, const vec3 p_WorldPosition)
{
    const vec3 specularDirection = normalize(p_Data.ViewPosition.xyz - p_WorldPosition);

    vec3 diffuseColor = vec3(0.0);
    vec3 specularColor = vec3(0.0);
    for (uint i = 0; i < p_Data.DirectionalLightCount; ++i)
    {
        const DirectionalLight dlight = directionalLights.Lights[i];
        const vec3 direction = vec3(dlight.PosY, dlight.PosY, dlight.PosZ);
        const float product = dot(p_Normal, direction);

        if (product > 0.0)
        {
            const float intensity = dlight.Intensity;
            const vec3 lightColor = unpackUnorm4x8(dlight.Color).xyz;
            diffuseColor += intensity * lightColor * product;

            const vec3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(p_Normal, halfVector), 0.0), p_Reflection.SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    for (uint i = 0; i < p_Data.PointLightCount; ++i)
    {
        const PointLight plight = pointLights.Lights[i];
        const vec3 direction = vec3(plight.PosX, plight.PosY, plight.PosZ) - p_WorldPosition;
        const float product = dot(p_Normal, direction);

        if (product > 0.0)
        {
            const vec3 lightColor = unpackUnorm4x8(plight.Color).xyz;
            const float radius = plight.Radius * plight.Radius;
            const float dirLen = dot(direction, direction);
            const float attenuation = radius / (dirLen + radius);
            const float intensity = attenuation * plight.Intensity;
            diffuseColor += intensity * lightColor * max(dot(p_Normal, normalize(direction)), 0.0);

            const vec3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(p_Normal, halfVector), 0.0), p_Reflection.SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    return p_Data.AmbientColor.xyz * p_Data.AmbientColor.w +
        p_Reflection.DiffuseContribution * diffuseColor + p_Reflection.SpecularContribution * specularColor;
}
#endif

#endif
