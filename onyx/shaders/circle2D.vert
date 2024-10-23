#version 460

layout(location = 0) out flat vec4 o_FragColor;
layout(location = 1) out vec2 o_LocalPosition;

struct InstanceData
{
    mat4 Transform;
    vec4 Color;
};

layout(set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData Instances[];
}
instanceBuffer;

const vec2 g_Positions[6] =
    vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

void main()
{
    gl_Position = instanceBuffer.Instances[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    gl_PointSize = 1.0;

    o_FragColor = instanceBuffer.Instances[gl_InstanceIndex].Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
}