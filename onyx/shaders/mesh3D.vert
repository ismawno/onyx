#version 450

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec3 o_FragNormal;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 colorAndNormalMatrix;
}
push;

void main()
{
    gl_Position = push.transform * vec4(i_Position, 1.0);
    gl_PointSize = 1.0;

    const mat3 normalMatrix = mat3(push.colorAndNormalMatrix);
    o_FragNormal = normalize(normalMatrix * i_Normal);
    o_FragColor = push.colorAndNormalMatrix[3];
}