#version 460

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;

layout(location = 0) out flat vec4 o_FragColor;
layout(location = 1) out vec3 o_FragNormal;
layout(location = 2) out vec3 o_WorldPosition;
layout(location = 3) out flat vec3 o_ViewPosition;

struct MaterialData
{
    float DiffuseContribution;
    float SpecularContribution;
    float SpecularSharpness;
};

layout(location = 4) out flat MaterialData o_Material;

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

void main()
{
    const vec4 worldPos = objectBuffer.Objects[gl_InstanceIndex].Transform * vec4(i_Position, 1.0);
    gl_Position = objectBuffer.Objects[gl_InstanceIndex].ProjectionView * worldPos;
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(objectBuffer.Objects[gl_InstanceIndex].NormalMatrix);
    o_FragNormal = normalize(normalMatrix * i_Normal);
    o_FragColor = objectBuffer.Objects[gl_InstanceIndex].Color;
    o_WorldPosition = worldPos.xyz;
    o_ViewPosition = objectBuffer.Objects[gl_InstanceIndex].ViewPosition.xyz;
    o_Material = objectBuffer.Objects[gl_InstanceIndex].Material;
}