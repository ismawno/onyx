#include "utils.slang"
struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    nointerpolation ArcData Arc;
    [[vk::location(4)]]
    nointerpolation f32v2 Fade;
    [[vk::location(5)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
}

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
CircleInstances3D g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
}

[[vk::push_constant]]
PushConstants g_PushData;


static const f32v2 g_Positions[6] =
{
    f32v2(-0.5, -0.5),
    f32v2( 0.5,  0.5),
    f32v2(-0.5,  0.5),
    f32v2(-0.5, -0.5),
    f32v2( 0.5, -0.5),
    f32v2( 0.5,  0.5)
};

[shader("vertex")]
VertexOutput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceData3D cdata = g_Instances[instanceIndex];
    const InstanceData3D data = cdata.Data;

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v2 scale = ComputeStencilScale3D(transform).xy;
    const f32v2 extruded = localPos - alignment + (normalize(localPos) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

    const f32v4 pos = f32v4(extruded, 0.0, 1.0);

    VertexOutput output;
    output.Position = mul(mul(pos, transform), g_PushData.ProjectionView);

    output.LocalPosition = localPos;

    output.Arc.CosSin = f32v4(cdata.Arc.LowerCos, cdata.Arc.LowerSin, cdata.Arc.UpperCos, cdata.Arc.UpperSin);
    output.Arc.AngleOverflow = cdata.Arc.AngleOverflow;
    output.Arc.Hollowness = cdata.Arc.Hollowness;

    output.Fade = f32v2(cdata.Fade.InnerFade, cdata.Fade.OuterFade);

    output.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);

    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    f32v4 color = input.FragColor;
    color.a = CircleFade(color.a, bounds, input.Fade);
    return color;
}
