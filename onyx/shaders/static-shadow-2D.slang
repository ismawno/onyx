#include "static.slang"
#include "material.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
}

struct FragInput
{
    f32v4 Position : SV_Position;
    [[vk::location(0)]]
    nointerpolation u32 MatId;
}

struct PushConstants
{
    f32m4 ProjectionView;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
StaticInstances2D g_Instances;

[[vk::binding(ONYX_BOUNDS2_BINDING, 0)]]
Bounds2D g_Bounds;

[shader("vertex")]
FragInput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const StaticInstanceData2D sdata = g_Instances[instanceIndex];
    const InstanceData2D data = sdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Transform);
    const f32v2 alignment = ComputeAlignment2D(data.Alignment, sdata.BoundsHandle, g_Bounds);

    FragInput finput;
    finput.Position = mul(mul(f32v4(input.Position - alignment, 0.0, 1.0), transform), g_PushData.ProjectionView);
    finput.MatId = GetAssetId(data.MatHandle);

    return finput;
}

[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
Materials2D g_Materials;

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    if (input.MatId == NullAsset || g_Materials[input.MatId].Occluder == 0)
        discard;
    return f32v4(1.f);
}
