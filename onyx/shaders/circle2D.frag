#version 450

layout(location = 0) in vec4 i_FragColor;
layout(location = 1) in vec2 i_LocalPosition;
layout(location = 0) out vec4 o_Color;

void main()
{
    if (dot(i_LocalPosition, i_LocalPosition) > 0.25)
        discard;
    o_Color = i_FragColor;
}