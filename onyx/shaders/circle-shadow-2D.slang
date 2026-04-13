#include "circle.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    nointerpolation ArcData Arc;

    f32v4 Position : SV_Position;
}

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
CircleInstances2D g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
}

[[vk::push_constant]]
PushConstants g_PushData;

[shader("vertex")]
VertexOutput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceData2D cdata = g_Instances[instanceIndex];
    const InstanceData2D data = cdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Transform);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];

    VertexOutput output;
    output.Position = mul(mul(f32v4(localPos - alignment, 0.f, 1.f), transform), g_PushData.ProjectionView);

    output.LocalPosition = localPos;

    output.Arc.CosSin = f32v4(cdata.Arc.LowerCos, cdata.Arc.LowerSin, cdata.Arc.UpperCos, cdata.Arc.UpperSin);
    output.Arc.AngleOverflow = cdata.Arc.AngleOverflow;
    output.Arc.Hollowness = cdata.Arc.Hollowness;

    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    return f32v4(1.f);
}
