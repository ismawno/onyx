#include "circle.slang"
#include "material.slang"

struct FragInput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    f32v2 WorldPosition;
    [[vk::location(3)]]
    nointerpolation u32 MatId;
    [[vk::location(4)]]
    nointerpolation ArcData Arc;
    [[vk::location(7)]]
    nointerpolation f32v2 Fade;
    [[vk::location(8)]]
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
CircleInstances2D g_Instances;

[shader("vertex")]
FragInput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceData2D cdata = g_Instances[instanceIndex];
    const InstanceData2D data = cdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Transform);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos - alignment, 0.0, 1.0), transform);

    FragInput finput;
    finput.Position = mul(worldPos, g_PushData.ProjectionView);
    finput.Position.z = ComputeDepth(data.DepthCounter);
    finput.TexCoord = g_TexCoords[vertexIndex];

    finput.LocalPosition = localPos;
    finput.WorldPosition = worldPos.xy;
    finput.MatId = GetAssetId(data.MatHandle);

    finput.Arc.CosSin = f32v4(cdata.Arc.LowerCos, cdata.Arc.LowerSin, cdata.Arc.UpperCos, cdata.Arc.UpperSin);
    finput.Arc.AngleOverflow = cdata.Arc.AngleOverflow;
    finput.Arc.Hollowness = cdata.Arc.Hollowness;

    finput.Fade = f32v2(cdata.Fade.InnerFade, cdata.Fade.OuterFade);
    finput.FragColor = unpackUnorm4x8ToFloat(data.FillColor);

    return finput;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
Materials2D g_Materials;

[[vk::binding(ONYX_PLIGHTS_BINDING, 0)]]
PointLights2D g_PointLights;

[[vk::binding(ONYX_SHADOW_SAMPLER_BINDING, 0)]]
SamplerState g_ShadowSampler;

[[vk::binding(ONYX_SHADOW_MAPS_BINDING, 0)]]
Texture1D g_ShadowMaps[ONYX_MAX_TEXTURE_MAPS];

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    const f32 alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    ResourceTable2D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.ShadowSampler = g_ShadowSampler;
    resources.ShadowMaps = g_ShadowMaps;
    return ComputeMaterialColor2D(input.FragColor, g_PushData.Light, input.MatId, input.WorldPosition, input.TexCoord, resources, alpha);
}
