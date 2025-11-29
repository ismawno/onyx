#version 460

layout(location = 0) in vec2 i_FragUV;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = vec4(i_FragUV, 0.5 * (1.0 + sin(i_FragUV.x * 10.0)), 1.0);
}
