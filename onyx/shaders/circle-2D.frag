#version 460

#include "utils.glsl"

layout(location = 0) in flat vec4 i_FragColor;
layout(location = 1) in smooth vec2 i_LocalPosition;
layout(location = 2) in flat ArcData i_Arc;
layout(location = 8) in flat FadeData i_Fade;

layout(location = 0) out vec4 o_Color;

void main()
{
    const CircleBounds bounds = GetCircleBounds(i_LocalPosition, i_Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, i_LocalPosition, i_Arc)) discard;

    o_Color = i_FragColor;
    o_Color.a = CircleFade(o_Color.a, bounds, i_Fade);
}
