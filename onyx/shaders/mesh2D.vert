#version 450

layout(location = 0) in vec2 position;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform Push
{
    mat4 transform;
    vec4 color;
}
push;

void main()
{
    gl_Position = push.transform * vec4(position, 0.0, 1.0);
    fragColor = push.color;
    gl_PointSize = 1.0;
}