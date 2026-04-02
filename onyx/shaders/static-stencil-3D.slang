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

[[vk::binding(0, 0)]]
StructuredBuffer<StaticInstanceData3D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
}

[[vk::push_constant]]
PushConstants g_PushData;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const StaticInstanceData3D data = g_Instances[instanceIndex];
    const f32m4 transform = ComputeTransform3D(data.Basis);

    const f32v3 scale = ComputeStencilScale3D(transform);
    const f32v3 extruded = input.Position + (normalize(input.Position) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

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
