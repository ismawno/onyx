#version 460

layout(location = 0) in flat vec4 i_FragColor;
layout(location = 1) in vec2 i_LocalPosition;
layout(location = 2) in flat vec4 i_ArcInfo;
layout(location = 3) in flat uint i_AngleOverflow;
layout(location = 4) in flat float i_Hollowness;

layout(location = 0) out vec4 o_Color;

void main()
{
    const float len = dot(i_LocalPosition, i_LocalPosition);
    if (len > 0.25 || len < 0.25 * i_Hollowness * i_Hollowness)
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

    o_Color = i_FragColor;
}