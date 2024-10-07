#version 450

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec2 o_LocalPosition;

layout(push_constant) uniform Push
{
    mat4 Transform;
    vec4 Color;
}
push;

const vec2 g_Positions[6] =
    vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

void main()
{
    gl_Position = push.Transform * vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    gl_PointSize = 1.0;

    o_FragColor = push.Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
}