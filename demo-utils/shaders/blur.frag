#version 460

layout(set = 0, binding = 0) uniform sampler2D i_FrameImage;

layout(location = 0) in vec2 i_FragUV;
layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform BlurData
{
    uint KernelSize;
    float width;
    float height;
}
blurData;

void main()
{
    const uint kernelRadius = uint(blurData.KernelSize) / 2;

    vec4 sumColor = vec4(0.0);
    float count = 0.0;

    for (uint y = -kernelRadius; y <= kernelRadius; y++)
        for (uint x = -kernelRadius; x <= kernelRadius; x++)
        {
            const vec2 offset = vec2(float(x) / blurData.width, float(y) / blurData.height);
            sumColor += texture(i_FrameImage, i_FragUV + offset);
            count += 1.0;
        }

    o_Color = sumColor / count;
}
