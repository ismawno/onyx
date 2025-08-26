#version 460

#include "utils.glsl"

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;

layout(location = 0) out smooth vec3 o_FragNormal;
layout(location = 1) out smooth vec3 o_WorldPosition;
layout(location = 2) out flat MaterialData3D o_Material;

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceDataFill3D Instances[];
}
instanceBuffer;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
    LightData3D LightData;
}
pdata;

void main()
{
    const InstanceDataFill3D data = instanceBuffer.Instances[gl_InstanceIndex];
    const mat4 transform = ComputeTransform3D(data.Basis);
    const vec4 worldPos = transform * vec4(i_Position, 1.0);

    gl_Position = pdata.ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = ComputeNormalMatrix(mat3(transform));

    o_FragNormal = normalize(normalMatrix * i_Normal);
    o_WorldPosition = worldPos.xyz;

    o_Material.Color = unpackUnorm4x8(data.Color);
    o_Material.Reflection = data.Reflection;
}
