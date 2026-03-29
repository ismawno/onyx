#include "utils.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
    f32v2 TexCoord : TEXCOORD0;
    f32v3 Normal : NORMAL;
    f32v4 Tangent : TANGENT;
    ParametricRegionFlags Region;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 WorldPosition;
    [[vk::location(1)]]
    f32v3 LocalPosition;
    [[vk::location(2)]]
    f32v2 TexCoord;
    [[vk::location(3)]]
    nointerpolation u32 MatHandle;
    [[vk::location(4)]]
    f32v4 FragColor;
    [[vk::location(5)]]
    nointerpolation u32v2 ShapeInfo;
    [[vk::location(6)]]
    f32m3 TBN;

    f32v4 Position : SV_Position;
}

[[vk::binding(0, 0)]]
StructuredBuffer<ParametricInstanceDataFill3D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    LightData3D Light;
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

    const ParametricInstanceDataFill3D pdata = g_Instances[instanceIndex];
    const StaticInstanceDataFill3D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v3 pos = GetParametricPosition3D(input.Position, input.Normal, params, input.Region);

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v4 worldPos = mul(f32v4(pos, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));

    const f32v3 N = normalize(mul(input.Normal, normalMatrix));
    const f32v3 T = normalize(mul(input.Tangent.xyz, normalMatrix));

    const f32v3 B = cross(N, T) * input.Tangent.w;
    output.TBN = f32m3(T, B, N);

    output.WorldPosition = worldPos.xyz;
    output.LocalPosition = input.Position;
    output.ShapeInfo = u32v2(params.Shape, input.Region);
    output.TexCoord = input.TexCoord;
    output.MatHandle = data.MatHandle;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

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
    if (!CheckParametricFragment3D(input.LocalPosition, input.ShapeInfo))
        discard;

    ResourceTable3D resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.DirLights = g_DirLights;
    return ComputeColor3D(input.FragColor, g_PushData.Light, input.MatHandle, input.WorldPosition, input.TexCoord, resources, 1.f, input.TBN, isFrontFacing);
}

