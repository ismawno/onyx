#version 460

#define ONYX_LIGHTS
#include "utils.glsl"

layout(location = 0) in flat vec3 i_FragNormal;
layout(location = 1) in smooth vec2 i_LocalPosition;
layout(location = 2) in smooth vec3 i_WorldPosition;
layout(location = 3) in flat ArcData i_Arc;
layout(location = 9) in flat FadeData i_Fade;
layout(location = 11) in flat MaterialData3D i_Material;

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
    LightData3D Light;
}
pdata;

void main()
{
    const CircleBounds bounds = GetCircleBounds(i_LocalPosition, i_Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, i_LocalPosition, i_Arc)) discard;

    vec3 normal = i_FragNormal;
    if (!gl_FrontFacing)
        normal = -normal;

    const LightData3D ldata = pdata.Light;
    const vec3 lcolor = GetLightColor(ldata, normal, i_Material.Reflection, i_WorldPosition);

    const float alpha = CircleFade(i_Material.Color.a, bounds, i_Fade);
    o_Color = i_Material.Color * vec4(lcolor, alpha);
}
