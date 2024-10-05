#version 450

layout(location = 0) in vec2 i_Position;
layout(location = 0) out vec4 o_FragColor;

layout(push_constant) uniform Push
{
    mat4 Transform;
    vec4 Color;
}
push;

void main()
{
    gl_Position = push.Transform * vec4(i_Position, 0.0, 1.0);
    gl_PointSize = 1.0;
    o_FragColor = push.Color;
}