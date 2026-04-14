#include "parametric.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    ParametricRegionFlags Region;
}

struct FragInput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    nointerpolation f32v4 FragColor;
    [[vk::location(2)]]
    nointerpolation u32v2 ShapeInfo;

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
ParametricInstances2D g_Instances;

[[vk::binding(ONYX_BOUNDS2_BINDING, 0)]]
Bounds2D g_Bounds;

[shader("vertex")]
FragInput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const ParametricInstanceData2D pdata = g_Instances[instanceIndex];
    const InstanceData2D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v2 alignment = ComputeAlignment2D(data.Alignment, pdata.BoundsHandle, g_Bounds);
    const f32v2 pos = GetParametricPosition2D(input.Position, params, input.Region, alignment);

    const f32m4 transform = ComputeTransform2D(data.Transform);

    const f32v2 scale = ComputeStencilScale2D(transform);
    const f32v2 extruded = pos - alignment + (normalize(input.Position) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

    FragInput finput;
    finput.Position = mul(mul(f32v4(extruded, 0.0, 1.0), transform), g_PushData.ProjectionView);
    finput.Position.z = ComputeDepth(data.DepthCounter);

    finput.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);
    finput.LocalPosition = input.Position;
    finput.ShapeInfo = u32v2(params.Shape, input.Region);

    return finput;
}

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    if (!CheckParametricFragment2D(input.LocalPosition, input.ShapeInfo))
        discard;

    return input.FragColor;
}
