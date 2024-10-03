#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform UBO
{
    vec4 lightDirection;
    float lightIntensity;
    float ambientIntensity;
}
ubo;

void main()
{
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    const float light = max(dot(normal, ubo.lightDirection.xyz) * ubo.lightIntensity, 0.0) + ubo.ambientIntensity;
    outColor = fragColor * light;
}