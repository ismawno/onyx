#include "glyph.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 AtlasCoord : TEXCOORD0;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 AtlasCoord;
    [[vk::location(1)]]
    f32v3 WorldPosition;
    [[vk::location(2)]]
    nointerpolation u32 SamplerId;
    [[vk::location(3)]]
    nointerpolation u32 AtlasId;

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
GlyphInstances3D g_Instances;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const GlyphInstanceData3D gdata = g_Instances[instanceIndex];
    const InstanceData3D data = gdata.Data;

    const f32m4 transform = ComputeTransform3D(data.Transform);

    VertexOutput output;

    const f32v4 worldPos = mul(f32v4(input.Position, 0.0, 1.0), transform);
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    output.AtlasCoord = input.AtlasCoord;
    output.WorldPosition = worldPos.xyz;
    output.SamplerId = GetAssetId(gdata.SamplerHandle);
    output.AtlasId = GetAssetId(gdata.AtlasHandle);

    return output;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[shader("fragment")]
f32 mainFS(const VertexOutput input) : SV_Depth
{
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    if (coverage <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;

    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;
}
