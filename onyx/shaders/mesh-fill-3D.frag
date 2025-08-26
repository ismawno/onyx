#version 460

#define ONYX_LIGHTS
#include "utils.glsl"

layout(location = 0) in smooth vec3 i_FragNormal;
layout(location = 1) in smooth vec3 i_WorldPosition;
layout(location = 2) in flat MaterialData3D i_Material;

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform PushConstantData
{
    mat4 ProjectionView;
    LightData3D LightData;
}
pdata;

void main()
{
    vec3 normal = normalize(i_FragNormal);
    if (!gl_FrontFacing)
        normal = -normal;

    const LightData3D ldata = pdata.LightData;
    const vec3 color = GetLightColor(ldata, normal, i_Material.Reflection, i_WorldPosition);

    o_Color = min(i_Material.Color * vec4(color, 1.0), 1.0);
}
