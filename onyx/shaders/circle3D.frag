#version 460

layout(location = 0) in flat vec3 i_FragNormal;
layout(location = 1) in vec3 i_LocalPosition;
layout(location = 2) in vec3 i_WorldPosition;
layout(location = 3) in flat vec3 i_ViewPosition;

struct MaterialData
{
    vec4 Color;
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
    if (dot(i_LocalPosition, i_LocalPosition) > 0.25)
        discard;

    vec3 normal = i_FragNormal;
    if (!gl_FrontFacing)
        normal = -normal;

    const vec3 specularDirection = normalize(i_ViewPosition - i_WorldPosition);

    vec3 diffuseColor = vec3(0.0);
    vec3 specularColor = vec3(0.0);
    for (uint i = 0; i < lightData.DirectionalLightCount; ++i)
    {
        const vec3 direction = directionalLights.Lights[i].DirectionAndIntensity.xyz;
        const float product = dot(normal, direction);

        if (product > 0.0)
        {
            const float intensity = directionalLights.Lights[i].DirectionAndIntensity.w;
            const vec3 lightColor = directionalLights.Lights[i].Color.xyz;
            diffuseColor += intensity * lightColor * product;

            const vec3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(normal, halfVector), 0.0), i_Material.SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    for (uint i = 0; i < lightData.PointLightCount; ++i)
    {
        const vec3 direction = pointLights.Lights[i].PositionAndIntensity.xyz - i_WorldPosition;
        const float product = dot(normal, direction);

        if (product > 0.0)
        {
            const vec3 lightColor = pointLights.Lights[i].Color.xyz;
            const float radius = pointLights.Lights[i].Radius * pointLights.Lights[i].Radius;
            const float len = dot(direction, direction);
            const float attenuation = radius / (len + radius);
            const float intensity = attenuation * pointLights.Lights[i].PositionAndIntensity.w;
            diffuseColor += intensity * lightColor * max(dot(normal, normalize(direction)), 0.0);

            const vec3 halfVector = normalize(direction + specularDirection);
            const float specular = pow(max(dot(normal, halfVector), 0.0), i_Material.SpecularSharpness);
            specularColor += intensity * specular * lightColor;
        }
    }

    const vec3 color = lightData.AmbientColor.xyz * lightData.AmbientColor.w +
                       i_Material.DiffuseContribution * diffuseColor + i_Material.SpecularContribution * specularColor;
    o_Color = min(i_Material.Color * vec4(color, 1.0), 1.0);
}