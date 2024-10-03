#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 colorAndNormalMatrix;
}
push;

void main()
{
    gl_Position = push.transform * vec4(position, 1.0);

    const mat3 normalMatrix = mat3(push.colorAndNormalMatrix);
    fragNormal = normalize(normalMatrix * normal);
    fragColor = push.colorAndNormalMatrix[3];

    gl_PointSize = 1.0;
}