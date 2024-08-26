#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 projection;
}
push;

void main()
{
    gl_Position = push.projection * push.transform * vec4(position, 1.0);
    frag_color = color;
    gl_PointSize = 1.0;
}