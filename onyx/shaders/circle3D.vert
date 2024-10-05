#version 450

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec3 o_FragNormal;
layout(location = 2) out vec3 o_LocalPosition;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 colorAndNormalMatrix;
}
push;

// A 3D CUBE
const vec3 g_Positions[8] =
    vec3[](vec3(-1.0, -1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(-1.0, 1.0, -1.0),
           vec3(-1.0, -1.0, 1.0), vec3(1.0, -1.0, 1.0), vec3(1.0, 1.0, 1.0), vec3(-1.0, 1.0, 1.0));

void main()
{
    gl_Position = push.transform * vec4(g_Positions[gl_VertexIndex], 1.0);
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(push.colorAndNormalMatrix);
    o_FragNormal = normalize(normalMatrix * g_Positions[gl_VertexIndex]);
    o_FragColor = push.colorAndNormalMatrix[3];
    o_LocalPosition = g_Positions[gl_VertexIndex];
}