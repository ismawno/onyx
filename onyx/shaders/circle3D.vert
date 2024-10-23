#version 460

layout(location = 0) out flat vec4 o_FragColor;
layout(location = 1) out flat vec3 o_FragNormal;
layout(location = 2) out vec3 o_LocalPosition;
layout(location = 3) out vec3 o_WorldPosition;
layout(location = 4) out flat vec3 o_ViewPosition;

struct MaterialData
{
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 5) out flat MaterialData o_Material;

struct ObjectData
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView;
    vec4 ViewPosition;
    vec4 Color;
    MaterialData Material;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
    ObjectData Objects[];
}
objectBuffer;

const vec3 g_Positions[6] = vec3[](vec3(-0.5, -0.5, 0.0), vec3(0.5, 0.5, 0.0), vec3(-0.5, 0.5, 0.0),
                                   vec3(-0.5, -0.5, 0.0), vec3(0.5, -0.5, 0.0), vec3(0.5, 0.5, 0.0));

void main()
{
    const vec4 worldPos = objectBuffer.Objects[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 1.0);
    gl_Position = objectBuffer.Objects[gl_InstanceIndex].ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(objectBuffer.Objects[gl_InstanceIndex].NormalMatrix);

    o_FragNormal = normalize(normalMatrix[2]); // Because normal is (0, 0, 1) for all vertices
    o_FragColor = objectBuffer.Objects[gl_InstanceIndex].Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
    o_WorldPosition = worldPos.xyz;
    o_ViewPosition = objectBuffer.Objects[gl_InstanceIndex].ViewPosition.xyz;
    o_Material = objectBuffer.Objects[gl_InstanceIndex].Material;
}