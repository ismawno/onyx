#version 460

#include "utils.glsl"

layout(location = 0) out flat vec4 o_FragColor;
layout(location = 1) out smooth vec2 o_LocalPosition;
layout(location = 2) out flat ArcData o_Arc;
layout(location = 8) out flat FadeData o_Fade;

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    CircleInstanceData2D Instances[];
}
instanceBuffer;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
}
pdata;

const vec2 g_Positions[6] =
    vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

void main()
{
    const CircleInstanceData2D data = instanceBuffer.Instances[gl_InstanceIndex];
    const mat4 transform = ComputeTransform2D(data.Data.Basis);

    const vec2 localPos = g_Positions[gl_VertexIndex];
    const vec4 pos = vec4(localPos, 0.0, 1.0);

    gl_Position = pdata.ProjectionView * transform * pos;
    gl_PointSize = 1.0;

    o_FragColor = unpackUnorm4x8(data.Data.Color);
    o_LocalPosition = localPos;
    o_Arc = data.Arc;
    o_Fade = data.Fade;
}
