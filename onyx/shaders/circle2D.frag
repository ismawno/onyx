#version 460

layout(location = 0) in flat vec4 i_FragColor;
layout(location = 1) in vec2 i_LocalPosition;
layout(location = 2) in flat vec4 i_ArcInfo;
layout(location = 3) in flat uint i_AngleOverflow;
layout(location = 4) in flat float i_Hollowness;
layout(location = 5) in flat float i_InnerFade;
layout(location = 6) in flat float i_OuterFade;

layout(location = 0) out vec4 o_Color;

void main()
{
    const float hollowness = i_Hollowness * i_Hollowness;
    const float len = 4.0 * dot(i_LocalPosition, i_LocalPosition);
    if (len > 1.0 || len < hollowness)
        discard;

    const vec2 lowerArc = i_ArcInfo.xy;
    const vec2 upperArc = i_ArcInfo.zw;

    const float crs1 = i_LocalPosition.x * lowerArc.y - i_LocalPosition.y * lowerArc.x;
    const float crs2 = i_LocalPosition.x * upperArc.y - i_LocalPosition.y * upperArc.x;

    if (i_AngleOverflow == 1)
    {
        if (crs1 > 0.0 && crs2 < 0.0)
            discard;
    }
    else
    {
        if (crs1 > 0.0 || crs2 < 0.0)
            discard;
    }

    const float innerFade = i_InnerFade * i_InnerFade;
    const float outerFade = 1.0 - i_OuterFade * i_OuterFade;
    const float shiftLen = (len - hollowness) / (1.0 - hollowness);

    o_Color = i_FragColor;
    if (shiftLen < innerFade)
        o_Color.a *= shiftLen / innerFade;
    if (shiftLen > outerFade)
        o_Color.a *= 1.0 - (shiftLen - outerFade) / (1.0 - outerFade);

    o_Color.a = clamp(o_Color.a, 0.0, 1.0);
}