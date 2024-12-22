#version 460

layout(set = 0, binding = 0) uniform sampler2D i_FrameImage;

layout(location = 0) in vec2 i_FragUV;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = texture(i_FrameImage, i_FragUV);
}
