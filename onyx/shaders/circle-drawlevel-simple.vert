#version 460

layout(location = 0) out flat vec4 o_FragColor;
layout(location = 1) out vec2 o_LocalPosition;
layout(location = 2) out flat vec4 o_ArcInfo;
layout(location = 3) out flat uint o_AngleOverflow;
layout(location = 4) out flat float o_Hollowness;
layout(location = 5) out flat float o_InnerFade;
layout(location = 6) out flat float o_OuterFade;

struct InstanceData
{
    mat4 Transform;
    vec4 Color;
    vec4 ArcInfo;
    uint AngleOverflow;
    float Hollowness;
    float InnerFade;
    float OuterFade;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData Instances[];
}
instanceBuffer;

layout(push_constant) uniform ProjectionViewData
{
    mat4 ProjectionView;
}
projectionView;

void main()
{
    const vec2 g_Positions[6] =
        vec2[](vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

    gl_Position = projectionView.ProjectionView * instanceBuffer.Instances[gl_InstanceIndex].Transform *
                  vec4(g_Positions[gl_VertexIndex], 0.0, 1.0);
    gl_PointSize = 1.0;

    o_FragColor = instanceBuffer.Instances[gl_InstanceIndex].Color;
    o_LocalPosition = g_Positions[gl_VertexIndex];
    o_ArcInfo = instanceBuffer.Instances[gl_InstanceIndex].ArcInfo;
    o_AngleOverflow = instanceBuffer.Instances[gl_InstanceIndex].AngleOverflow;
    o_Hollowness = instanceBuffer.Instances[gl_InstanceIndex].Hollowness;
    o_InnerFade = instanceBuffer.Instances[gl_InstanceIndex].InnerFade;
    o_OuterFade = instanceBuffer.Instances[gl_InstanceIndex].OuterFade;
}