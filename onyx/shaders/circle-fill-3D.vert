#version 460

#include "utils.glsl"

layout(location = 0) out flat vec3 o_FragNormal;
layout(location = 1) out smooth vec2 o_LocalPosition;
layout(location = 2) out smooth vec3 o_WorldPosition;
layout(location = 3) out flat ArcData o_Arc;
layout(location = 9) out flat FadeData o_Fade;
layout(location = 11) out flat MaterialData3D o_Material;

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    CircleInstanceDataFill3D Instances[];
}
instanceBuffer;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
    LightData3D LightData;
}
pdata;

const vec2 g_Positions[6] =
    vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5));

void main()
{
    const CircleInstanceDataFill3D data = instanceBuffer.Instances[gl_InstanceIndex];
    const mat4 transform = ComputeTransform3D(data.Data.Basis);

    const vec2 localPos = g_Positions[gl_VertexIndex];
    const vec4 worldPos = transform * vec4(localPos, 0.0, 1.0);

    gl_Position = pdata.ProjectionView * worldPos;
    gl_PointSize = 1.0;

    o_FragNormal = ComputeTrivialWorldNormal(mat3(transform));
    o_LocalPosition = g_Positions[gl_VertexIndex];
    o_WorldPosition = worldPos.xyz;
    o_Arc = data.Arc;
    o_Fade = data.Fade;

    o_Material.Color = unpackUnorm4x8(data.Data.Color);
    o_Material.Reflection = data.Data.Reflection;
}
