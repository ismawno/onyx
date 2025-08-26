#version 460

#include "utils.glsl"

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;

layout(location = 0) out flat vec4 o_FragColor;

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceDataStencil3D Instances[];
}
instanceBuffer;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
}
pdata;

void main()
{
    const InstanceDataStencil3D data = instanceBuffer.Instances[gl_InstanceIndex];
    const mat4 transform = ComputeTransform3D(data.Basis);

    gl_Position = pdata.ProjectionView * transform * vec4(i_Position, 1.0);
    gl_PointSize = 1.0;

    o_FragColor = unpackUnorm4x8(data.Color);
}
