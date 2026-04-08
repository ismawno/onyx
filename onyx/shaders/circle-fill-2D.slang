#include "circle.slang"
#include "material.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    f32v2 WorldPosition;
    [[vk::location(3)]]
    nointerpolation u32 MatHandle;
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
VertexOutput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceData2D cdata = g_Instances[instanceIndex];
    const InstanceData2D data = cdata.Data;

    const f32m4 transform = ComputeTransform2D(data.Basis);
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos - alignment, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.Position.z = ComputeDepth(data.DepthCounter);
    output.TexCoord = g_TexCoords[vertexIndex];

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xy;
    output.MatHandle = data.MatHandle;

    output.Arc.CosSin = f32v4(cdata.Arc.LowerCos, cdata.Arc.LowerSin, cdata.Arc.UpperCos, cdata.Arc.UpperSin);
    output.Arc.AngleOverflow = cdata.Arc.AngleOverflow;
    output.Arc.Hollowness = cdata.Arc.Hollowness;

    output.Fade = f32v2(cdata.Fade.InnerFade, cdata.Fade.OuterFade);
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
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    const f32 alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    ResourceTable2D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    return ComputeMaterialColor2D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.TexCoord, resources, alpha);
}
