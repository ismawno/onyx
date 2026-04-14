#include "parametric.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
    f32v3 Normal : NORMAL;
    ParametricRegionFlags Region;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 LocalPosition;
    [[vk::location(1)]]
    f32v3 WorldPosition;
    [[vk::location(2)]]
    nointerpolation u32v2 ShapeInfo;

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
ParametricInstances3D g_Instances;

[[vk::binding(ONYX_BOUNDS3_BINDING, 0)]]
Bounds3D g_Bounds;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const ParametricInstanceData3D pdata = g_Instances[instanceIndex];
    const InstanceData3D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v3 pos = GetParametricPosition3D(input.Position, input.Normal, params, input.Region);

    const f32m4 transform = ComputeTransform3D(data.Transform);
    const f32v3 alignment = ComputeAlignment3D(data.Alignment, pdata.BoundsHandle, g_Bounds);

    VertexOutput output;

    const f32v4 worldPos = mul(f32v4(pos - alignment, 1.0), transform);

    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.WorldPosition = worldPos.xyz;
    output.LocalPosition = input.Position;
    output.ShapeInfo = u32v2(params.Shape, input.Region);

    return output;
}

[shader("fragment")]
f32 mainFS(const VertexOutput input) : SV_Depth
{
    if (!CheckParametricFragment3D(input.LocalPosition, input.ShapeInfo))
        discard;

    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;
}
