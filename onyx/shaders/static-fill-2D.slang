#include "utils.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 TexCoord : TEXCOORD0;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 WorldPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    nointerpolation u32 MatHandle;
    [[vk::location(3)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
    LightData2D Light;
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
    const f32v4 worldPos = mul(f32v4(input.Position - alignment, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.Position.z = ComputeDepth(data.DepthCounter);

    output.TexCoord = input.TexCoord;

    output.WorldPosition = worldPos.xy;
    output.MatHandle = data.MatHandle;
    output.FragColor = unpackUnorm4x8ToFloat(data.FillColor);

    return output;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
Materials2D g_Materials;

[[vk::binding(ONYX_PLIGHTS_BINDING, 0)]]
PointLights2D g_PointLights;

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    ResourceTable2D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    return ComputeColor2D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.TexCoord, resources, 1.f);
}
