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
    nointerpolation u32 SamplerId;
    [[vk::location(2)]]
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
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const GlyphInstanceData2D gdata = g_Instances[instanceIndex];
    const InstanceData2D data = gdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Transform);

    VertexOutput output;
    output.Position = mul(mul(f32v4(input.Position, 0.0, 1.0), transform), g_PushData.ProjectionView);

    output.AtlasCoord = input.AtlasCoord;
    output.SamplerId = GetAssetId(gdata.SamplerHandle);
    output.AtlasId = GetAssetId(gdata.AtlasHandle);

    return output;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    if (coverage <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;
    return f32v4(1.f, 0.f, 0.f, 0.f);
}
