#version 450

layout(location = 0) in vec2 position;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 colorAndNormalMatrix;
}
push;

layout(binding = 0, set = 0) uniform UBO
{
    mat4 projection;
    vec4 lightDirection;
    float lightIntensity;
    float ambientIntensity;
}
ubo;

void main()
{
    gl_Position = ubo.projection * push.transform * vec4(position, 0.0, 1.0);
    fragColor = push.colorAndNormalMatrix[3] * (ubo.lightIntensity + ubo.ambientIntensity);
    gl_PointSize = 1.0;
}