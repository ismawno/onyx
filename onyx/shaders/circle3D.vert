#version 460

layout(location = 0) out flat vec3 o_FragNormal;
layout(location = 1) out vec2 o_LocalPosition;
layout(location = 2) out vec3 o_WorldPosition;
layout(location = 3) out flat vec3 o_ViewPosition;
layout(location = 4) out flat vec4 o_ArcInfo;
layout(location = 5) out flat uint o_AngleOverflow;

struct MaterialData
{
    vec4 Color;
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 6) out flat MaterialData o_Material;

struct InstanceData
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView;
    vec4 ViewPosition;
    MaterialData Material;
    vec4 ArcInfo;
    uint AngleOverflow;
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
    const vec4 worldPos =
        instanceBuffer.Instances[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    gl_Position = instanceBuffer.Instances[gl_InstanceIndex].ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(instanceBuffer.Instances[gl_InstanceIndex].NormalMatrix);

    o_FragNormal = normalize(normalMatrix[2]); // Because normal is (0, 0, 1) for all vertices
    o_LocalPosition = g_Positions[gl_VertexIndex];
    o_WorldPosition = worldPos.xyz;
    o_ViewPosition = instanceBuffer.Instances[gl_InstanceIndex].ViewPosition.xyz;
    o_ArcInfo = instanceBuffer.Instances[gl_InstanceIndex].ArcInfo;
    o_AngleOverflow = instanceBuffer.Instances[gl_InstanceIndex].AngleOverflow;
    o_Material = instanceBuffer.Instances[gl_InstanceIndex].Material;
}