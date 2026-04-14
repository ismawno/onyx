#include "circle.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v3 WorldPosition;
    [[vk::location(2)]]
    nointerpolation ArcData Arc;

    f32v4 Position : SV_Position;
}

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
CircleInstances3D g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
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

    const CircleInstanceData3D cdata = g_Instances[instanceIndex];
    const InstanceData3D data = cdata.Data;

    const f32m4 transform = ComputeTransform3D(data.Transform);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos - alignment, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xyz;

    output.Arc.CosSin = f32v4(cdata.Arc.LowerCos, cdata.Arc.LowerSin, cdata.Arc.UpperCos, cdata.Arc.UpperSin);
    output.Arc.AngleOverflow = cdata.Arc.AngleOverflow;
    output.Arc.Hollowness = cdata.Arc.Hollowness;

    return output;
}

[shader("fragment")]
f32 mainFS(const VertexOutput input) : SV_Depth
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;
}
