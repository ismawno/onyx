#version 460

layout(location = 0) in vec4 i_FragColor;
layout(location = 1) in vec3 i_FragNormal;

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform Light
{
    vec4 Direction;
    float Intensity;
    float AmbientIntensity;
}
light;

void main()
{
    vec3 normal = normalize(i_FragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    const float light = max(dot(normal, light.Direction.xyz) * light.Intensity, 0.0) + light.AmbientIntensity;
    o_Color = i_FragColor * light;
}