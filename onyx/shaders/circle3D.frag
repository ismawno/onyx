#version 450

layout(location = 0) in vec4 i_FragColor;
layout(location = 1) in vec3 i_FragNormal;
layout(location = 2) in vec3 i_LocalPosition;

layout(location = 0) out vec4 o_Color;

layout(binding = 0, set = 0) uniform UBO
{
    vec4 LightDirection;
    float LightIntensity;
    float AmbientIntensity;
}
ubo;

void main()
{
    if (abs(dot(i_LocalPosition, i_LocalPosition)) > 1.0)
        discard;

    vec3 normal = normalize(i_FragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    const float light = max(dot(normal, ubo.LightDirection.xyz) * ubo.LightIntensity, 0.0) + ubo.AmbientIntensity;
    o_Color = i_FragColor * light;
}