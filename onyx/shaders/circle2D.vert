#version 460

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec2 o_LocalPosition;

struct ObjectData
{
    mat4 Transform;
    vec4 Color;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
    ObjectData Objects[];
}
objectBuffer;

const vec2 g_Positions[6] =
    vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

void main()
{
    gl_Position = objectBuffer.Objects[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    gl_PointSize = 1.0;

    o_FragColor = objectBuffer.Objects[gl_InstanceIndex].Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
}