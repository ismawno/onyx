#include "static.slang"
#include "common.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
}

struct VertexOutput
{
    [[vk::location(0)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
StaticInstances2D g_Instances;

[[vk::binding(ONYX_BOUNDS2_BINDING, 0)]]
Bounds2D g_Bounds;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const StaticInstanceData2D sdata = g_Instances[instanceIndex];
    const InstanceData2D data = sdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Basis);
    const f32v2 alignment = ComputeAlignment2D(data.Alignment, sdata.BoundsHandle, g_Bounds);
    const f32v2 center = GetCenter2D(sdata.BoundsHandle, g_Bounds);

    const f32v2 scale = ComputeStencilScale2D(transform);
    const f32v2 extruded = input.Position - alignment + (normalize(input.Position - center) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

    VertexOutput output;
    output.Position = mul(mul(f32v4(extruded, 0.0, 1.0), transform), g_PushData.ProjectionView);
    output.Position.z = ComputeDepth(data.DepthCounter);
    output.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);

    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    return input.FragColor;
}
