#version 460

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec3 o_FragNormal;
layout(location = 2) out vec3 o_LocalPosition;

struct ObjectData
{
    mat4 Transform;
    mat4 NormalMatrix;
    vec4 Color;
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
    gl_Position = objectBuffer.Objects[gl_InstanceIndex].Transform * vec4(g_Positions[gl_VertexIndex], 1.0);
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(objectBuffer.Objects[gl_InstanceIndex].NormalMatrix);
    o_FragNormal = normalize(normalMatrix * vec3(0.0, 0.0, 1.0)); // Because normal is (0, 0, 1) for all vertices
    o_FragColor = objectBuffer.Objects[gl_InstanceIndex].Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
}