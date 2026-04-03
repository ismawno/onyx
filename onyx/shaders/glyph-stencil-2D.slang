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

struct PushConstants
{
    f32m4 ProjectionView;
    f32 OutlineMultiplier;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
GlyphInstances2D g_Instances;

[[vk::binding(1, 0)]]
Bounds2D g_Bounds;

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
    const f32v2 alignment = ComputeAlignment2D(data.Alignment, gdata.BoundsHandle, g_Bounds);
    const f32v2 center = GetCenter2D(gdata.BoundsHandle, g_Bounds);

    const f32v2 scale = ComputeStencilScale2D(transform);
    const f32v2 extruded = input.Position - alignment + (normalize(input.Position - center) * data.OutlineWidth * g_PushData.OutlineMultiplier) / scale;

    VertexOutput output;
    output.Position = mul(mul(f32v4(extruded, 0.0, 1.0), transform), g_PushData.ProjectionView);
    output.Position.z = ComputeDepth(data.DepthCounter);

    output.AtlasCoord = input.AtlasCoord;
    output.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);
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
    return f32v4(input.FragColor.rgb, input.FragColor.a * coverage);
}
