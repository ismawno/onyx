#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform Push
{
    mat4 transform;
    mat4 projection;
}
push;

void main()
{
    out_color = frag_color;
}