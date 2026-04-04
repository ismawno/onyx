#include "utils.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 AtlasCoord : TEXCOORD0;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 WorldPosition;
    [[vk::location(1)]]
    f32v2 AtlasCoord;
    [[vk::location(2)]]
    nointerpolation u32 MatHandle;
    [[vk::location(3)]]
    nointerpolation u32 SamplerId;
    [[vk::location(4)]]
    nointerpolation u32 AtlasId;
    [[vk::location(5)]]
    f32v4 FragColor;
    [[vk::location(6)]]
    f32m3 TBN;

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
    LightData3D Light;
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

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v4 worldPos = mul(f32v4(input.Position, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));

    const f32v3 N = normalize(normalMatrix[2]);
    const f32v3 T = normalize(normalMatrix[0]);

    const f32v3 B = cross(N, T);
    output.TBN = f32m3(T, B, N);

    output.WorldPosition = worldPos.xyz;
    output.AtlasCoord = input.AtlasCoord;
    output.MatHandle = data.MatHandle;
    output.SamplerId = GetAssetId(gdata.SamplerHandle);
    output.AtlasId = GetAssetId(gdata.AtlasHandle);
    output.FragColor = unpackUnorm4x8ToFloat(data.FillColor);

    return output;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
Materials3D g_Materials;

[[vk::binding(ONYX_PLIGHTS_BINDING, 0)]]
PointLights3D g_PointLights;

[[vk::binding(ONYX_DLIGHTS_BINDING, 0)]]
DirectionalLights g_DirLights;

[shader("fragment")]
f32v4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
{
    const f32 coverage = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoord);
    if (coverage <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;
    ResourceTable3D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.DirLights = g_DirLights;

    return ComputeColor3D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.AtlasCoord, resources, coverage, input.TBN, isFrontFacing);
}

