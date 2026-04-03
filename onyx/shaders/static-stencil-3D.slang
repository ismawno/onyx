#include "utils.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
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

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v3 alignment = ComputeAlignment3D(data.Alignment, sdata.BoundsHandle, g_Bounds);
    const f32v3 center = GetCenter3D(sdata.BoundsHandle, g_Bounds);

    const f32v3 scale = ComputeStencilScale3D(transform);
    const f32v3 extruded = input.Position - alignment + (normalize(input.Position - center) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

    VertexOutput output;

    output.Position = mul(mul(f32v4(extruded, 1.0), transform), g_PushData.ProjectionView);

    output.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);
    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    return input.FragColor;
}
