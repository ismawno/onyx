#include "static.slang"
#include "common.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 WorldPosition;

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
StaticInstances3D g_Instances;

[[vk::binding(ONYX_BOUNDS3_BINDING, 0)]]
Bounds3D g_Bounds;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const StaticInstanceData3D sdata = g_Instances[instanceIndex];
    const InstanceData3D data = sdata.Data;

    const f32m4 transform = ComputeTransform3D(data.Transform);
    const f32v3 alignment = ComputeAlignment3D(data.Alignment, sdata.BoundsHandle, g_Bounds);

    VertexOutput output;

    const f32v4 worldPos = mul(f32v4(input.Position - alignment, 1.0), transform);
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.WorldPosition = worldPos.xyz;
    return output;
}

[shader("fragment")]
f32 mainFS(const VertexOutput input) : SV_Depth
{
    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;
}

