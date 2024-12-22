#version 460

layout(set = 0, binding = 0) uniform sampler2D myTexture;

layout(location = 0) in vec2 i_FragUV;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = texture(myTexture, i_FragUV);
}
