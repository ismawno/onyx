#include "utils.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 AtlasCoord : TEXCOORD0;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 WorldPosition;
    [[vk::location(1)]]
    f32v2 AtlasCoord;
    [[vk::location(2)]]
    nointerpolation u32 MatHandle;
    [[vk::location(3)]]
    nointerpolation u32 SamplerId;
    [[vk::location(4)]]
    nointerpolation u32 AtlasId;
    [[vk::location(5)]]
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
GlyphInstances2D g_Instances;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const GlyphInstanceData2D gdata = g_Instances[instanceIndex];
    const InstanceData2D data = gdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Basis);
    const f32v4 worldPos = mul(f32v4(input.Position, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.Position.z = ComputeDepth(data.DepthCounter);

    output.AtlasCoord = input.AtlasCoord;
    output.SamplerId = GetAssetId(gdata.SamplerHandle);
    output.AtlasId = GetAssetId(gdata.AtlasHandle);

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
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    if (coverage <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;
    ResourceTable2D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;

    return ComputeColor2D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.AtlasCoord, resources, coverage);
}
