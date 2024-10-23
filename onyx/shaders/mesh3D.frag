#version 460

layout(location = 0) in flat vec4 i_FragColor;
layout(location = 1) in vec3 i_FragNormal;
layout(location = 2) in vec3 i_WorldPosition;
layout(location = 3) in flat vec3 i_ViewPosition;

struct MaterialData
{
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 4) in flat MaterialData i_Material;

layout(location = 0) out vec4 o_Color;

struct DirectionalLight
{
    vec4 DirectionAndIntensity;
    vec4 Color;
};
struct PointLight
{
    vec4 PositionAndIntensity;
    vec4 Color;
    float Radius;
};

layout(set = 1, binding = 0) readonly buffer DirectionalLights
{
    DirectionalLight Lights[];
}
directionalLights;

layout(set = 1, binding = 1) readonly buffer PointLights
{
    PointLight Lights[];
}
pointLights;

layout(push_constant) uniform LightData
{
    vec4 AmbientColor;
    uint DirectionalLightCount;
    uint PointLightCount;
    uint _Padding[2];
}
lightData;

void main()
{
    vec3 normal = normalize(i_FragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    const vec3 specularDirection = normalize(i_ViewPosition - i_WorldPosition);

    vec3 diffuseColor = lightData.AmbientColor.xyz * lightData.AmbientColor.w;
    vec3 specularColor = vec3(0.0);
    for (uint i = 0; i < lightData.DirectionalLightCount; ++i)
    {
        const float intensity = directionalLights.Lights[i].DirectionAndIntensity.w;
        const vec3 direction = directionalLights.Lights[i].DirectionAndIntensity.xyz;
        const vec3 lightColor = directionalLights.Lights[i].Color.xyz;

        diffuseColor += intensity * lightColor * max(dot(normal, direction), 0.0);

        const vec3 halfVector = normalize(direction + specularDirection);
        const float specular = pow(max(dot(normal, halfVector), 0.0), i_Material.SpecularSharpness);
        specularColor += intensity * specular * lightColor;
    }

    for (uint i = 0; i < lightData.PointLightCount; ++i)
    {
        const vec3 direction = pointLights.Lights[i].PositionAndIntensity.xyz - i_WorldPosition;
        const vec3 lightColor = pointLights.Lights[i].Color.xyz;
        const float intensity = pointLights.Lights[i].PositionAndIntensity.w;
        const float radius = pointLights.Lights[i].Radius;
        const float attenuation = clamp(1.0 - dot(direction, direction) / (radius * radius), 0.0, 1.0);

        diffuseColor += intensity * attenuation * lightColor * max(dot(normal, normalize(direction)), 0.0);

        const vec3 halfVector = normalize(direction + specularDirection);
        const float specular = pow(max(dot(normal, halfVector), 0.0), i_Material.SpecularSharpness);
        specularColor += intensity * attenuation * specular * lightColor;
    }

    const vec3 color = i_Material.DiffuseContribution * diffuseColor + i_Material.SpecularContribution * specularColor;
    o_Color = min(i_FragColor * vec4(color, 1.0), 1.0);
}