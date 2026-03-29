#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    f32v3 WorldPosition;
    [[vk::location(3)]]
    nointerpolation u32 MatHandle;
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

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataFill3D, Std430DataLayout> g_InstanceBuffer;

struct PushConstants
{
    f32m4 ProjectionView;
    LightData3D Light;
}

[[vk::push_constant]]
PushConstants g_PushData;


static const f32v2 g_Positions[6] =
{
    f32v2(-0.5, -0.5),
    f32v2( 0.5, -0.5),
    f32v2(-0.5,  0.5),
    f32v2( 0.5, -0.5),
    f32v2( 0.5,  0.5),
    f32v2(-0.5,  0.5),
};

static const f32v2 g_TexCoords[6] =
{
    f32v2(0.0, 1.0),
    f32v2(1.0, 1.0),
    f32v2(0.0, 0.0),
    f32v2(1.0, 1.0),
    f32v2(1.0, 0.0),
    f32v2(0.0, 0.0)
};

[shader("vertex")]
VertexOutput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceDataFill3D data = g_InstanceBuffer[instanceIndex];
    const f32m4 transform = ComputeTransform3D(data.Data.Basis);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = g_TexCoords[vertexIndex];

    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));
    const f32v3 N = normalize(normalMatrix[2]);
    const f32v3 T = normalize(normalMatrix[0]);
    const f32v3 B = cross(N, T);
    output.TBN = f32m3(T, B, N);

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xyz;
    output.MatHandle = data.Data.MatHandle;

    output.Arc.CosSin = f32v4(data.Arc.LowerCos, data.Arc.LowerSin, data.Arc.UpperCos, data.Arc.UpperSin);
    output.Arc.AngleOverflow = data.Arc.AngleOverflow;
    output.Arc.Hollowness = data.Arc.Hollowness;

    output.Fade = f32v2(data.Fade.InnerFade, data.Fade.OuterFade);

    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

    return output;
}

[[vk::binding(1, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(2, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(3, 0)]]
Materials3D g_Materials[ONYX_MAX_ASSET_POOLS];

[[vk::binding(4, 0)]]
PointLights3D g_PointLights;

[[vk::binding(5, 0)]]
DirectionalLights g_DirLights;

[shader("fragment")]
f32v4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
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
    return ComputeColor3D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.TexCoord, resources, alpha, input.TBN, isFrontFacing);
}

