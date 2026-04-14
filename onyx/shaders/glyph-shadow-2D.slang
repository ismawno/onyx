#include "glyph.slang"
#include "material.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 AtlasCoord : TEXCOORD0;
}

struct FragInput
{
    [[vk::location(0)]]
    f32v2 AtlasCoord;
    [[vk::location(1)]]
    nointerpolation u32 MatId;
    [[vk::location(2)]]
    nointerpolation u32 SamplerId;
    [[vk::location(3)]]
    nointerpolation u32 AtlasId;

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
GlyphInstances2D g_Instances;

[shader("vertex")]
FragInput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const GlyphInstanceData2D gdata = g_Instances[instanceIndex];
    const InstanceData2D data = gdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Transform);

    FragInput finput;
    finput.Position = mul(mul(f32v4(input.Position, 0.0, 1.0), transform), g_PushData.ProjectionView);
    finput.MatId = GetAssetId(data.MatHandle);

    finput.AtlasCoord = input.AtlasCoord;
    finput.SamplerId = GetAssetId(gdata.SamplerHandle);
    finput.AtlasId = GetAssetId(gdata.AtlasHandle);

    return finput;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
Materials2D g_Materials;

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    if (coverage <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;
    return f32v4(input.MatId != NullAsset && g_Materials[input.MatId].Occluder == 1);
}
