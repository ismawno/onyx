#version 460

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;

layout(location = 0) out vec3 o_FragNormal;
layout(location = 1) out vec3 o_WorldPosition;

struct MaterialData
{
    vec4 Color;
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 2) out flat MaterialData o_Material;

struct InstanceData
{
    mat4 Transform;
    mat4 NormalMatrix;
    MaterialData Material;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData Instances[];
}
instanceBuffer;

layout(push_constant) uniform ProjectionViewData
{
    mat4 ProjectionView;
    vec4 ViewPosition;
    vec4 AmbientColor;
    uint DirectionalLightCount;
    uint PointLightCount;
    uint _Padding[2];
}
projectionView;

void main()
{
    const vec4 worldPos = instanceBuffer.Instances[gl_InstanceIndex].Transform * vec4(i_Position, 1.0);
    gl_Position = projectionView.ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(instanceBuffer.Instances[gl_InstanceIndex].NormalMatrix);
    o_FragNormal = normalize(normalMatrix * i_Normal);
    o_WorldPosition = worldPos.xyz;
    o_Material = instanceBuffer.Instances[gl_InstanceIndex].Material;
}