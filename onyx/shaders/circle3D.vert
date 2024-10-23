#version 460

layout(location = 0) out flat vec3 o_FragNormal;
layout(location = 1) out vec3 o_LocalPosition;
layout(location = 2) out vec3 o_WorldPosition;
layout(location = 3) out flat vec3 o_ViewPosition;

struct MaterialData
{
    vec4 Color;
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 4) out flat MaterialData o_Material;

struct InstanceData
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView;
    vec4 ViewPosition;
    MaterialData Material;
};

layout(set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData Instances[];
}
instanceBuffer;

const vec3 g_Positions[6] = vec3[](vec3(-0.5, -0.5, 0.0), vec3(0.5, 0.5, 0.0), vec3(-0.5, 0.5, 0.0),
                                   vec3(-0.5, -0.5, 0.0), vec3(0.5, -0.5, 0.0), vec3(0.5, 0.5, 0.0));

void main()
{
    const vec4 worldPos = instanceBuffer.Instances[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 1.0);
    gl_Position = instanceBuffer.Instances[gl_InstanceIndex].ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(instanceBuffer.Instances[gl_InstanceIndex].NormalMatrix);

    o_FragNormal = normalize(normalMatrix[2]); // Because normal is (0, 0, 1) for all vertices
    o_LocalPosition = g_Positions[gl_VertexIndex];
    o_WorldPosition = worldPos.xyz;
    o_ViewPosition = instanceBuffer.Instances[gl_InstanceIndex].ViewPosition.xyz;
    o_Material = instanceBuffer.Instances[gl_InstanceIndex].Material;
}