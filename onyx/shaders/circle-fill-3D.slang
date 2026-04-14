#include "circle.slang"
#include "material.slang"

struct FragInput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    f32v3 WorldPosition;
    [[vk::location(3)]]
    nointerpolation u32 MatId;
    [[vk::location(4)]]
    nointerpolation ArcData Arc;
    [[vk::location(7)]]
    nointerpolation f32v2 Fade;
    [[vk::location(8)]]
    nointerpolation f32v4 FragColor;
    [[vk::location(9)]]
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
CircleInstances3D g_Instances;

[shader("vertex")]
FragInput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceData3D cdata = g_Instances[instanceIndex];
    const InstanceData3D data = cdata.Data;

    const f32m4 transform = ComputeTransform3D(data.Transform);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos - alignment, 0.0, 1.0), transform);

    FragInput finput;
    finput.Position = mul(worldPos, g_PushData.ProjectionView);
    finput.TexCoord = g_TexCoords[vertexIndex];

    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));
    const f32v3 N = normalize(normalMatrix[2]);
    const f32v3 T = normalize(normalMatrix[0]);
    const f32v3 B = cross(N, T);
    finput.TBN = f32m3(T, B, N);

    finput.LocalPosition = localPos;
    finput.WorldPosition = worldPos.xyz;
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
Materials3D g_Materials;

[[vk::binding(ONYX_PLIGHTS_BINDING, 0)]]
PointLights3D g_PointLights;

[[vk::binding(ONYX_DLIGHTS_BINDING, 0)]]
DirectionalLights g_DirLights;

[[vk::binding(ONYX_SHADOW_SAMPLER_BINDING, 0)]]
SamplerComparisonState g_ShadowSampler;

[[vk::binding(ONYX_POINT_MAPS_BINDING, 0)]]
TextureCube g_PointMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_DIRECTIONAL_MAPS_BINDING, 0)]]
Texture2D g_DirectionalMaps[ONYX_MAX_TEXTURE_MAPS];

[shader("fragment")]
f32v4 mainFS(const FragInput input, const bool isFrontFacing : SV_IsFrontFace)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    const f32 alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    ResourceTable3D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.DirLights = g_DirLights;
    resources.ShadowSampler = g_ShadowSampler;
    resources.PointMaps = g_PointMaps;
    resources.DirectionalMaps = g_DirectionalMaps;
    return ComputeMaterialColor3D(input.FragColor, g_PushData.Light, input.MatId, input.WorldPosition, input.TexCoord, resources, alpha, input.TBN, isFrontFacing);
}

