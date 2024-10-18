#version 460

layout(location = 0) in vec4 i_FragColor;
layout(location = 1) in vec3 i_FragNormal;
layout(location = 2) in vec3 i_LocalPosition;

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform Light
{
    vec4 Directionals[7];
    float AmbientIntensity;
    uint LightCount;
    uint _Padding[2];
}
light;

void main()
{
    if (dot(i_LocalPosition, i_LocalPosition) > 0.25)
        discard;

    vec3 normal = normalize(i_FragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    float finalLight = light.AmbientIntensity;
    for (uint i = 0; i < light.LightCount; ++i)
        finalLight += max(dot(normal, light.Directionals[i].xyz) * light.Directionals[i].w, 0.0);

    o_Color = i_FragColor * finalLight;
}