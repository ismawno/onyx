#include "utils.slang"

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
    [[vk::location(3)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
}

[[vk::binding(0, 0)]]
StructuredBuffer<GlyphInstanceDataStencil2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
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

    const GlyphInstanceDataStencil2D gdata = g_Instances[instanceIndex];
    const StaticInstanceDataStencil2D data = gdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Basis);

    const f32v2 scale = ComputeStencilScale2D(transform);
    const f32v2 extruded = input.Position + (normalize(input.Position) * data.OutlineWidth) / scale;

    VertexOutput output;
    output.Position = mul(mul(f32v4(extruded, 0.0, 1.0), transform), g_PushData.ProjectionView);
    output.AtlasCoord = input.AtlasCoord;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);
    output.SamplerId = GetAssetId(gdata.SamplerHandle);
    output.AtlasId = GetAssetId(gdata.AtlasHandle);

    return output;
}

[[vk::binding(1, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(2, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    return f32v4(input.FragColor.rbg, input.FragColor.a * coverage);
}
