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

struct PushConstants
{
    f32m4 ProjectionView;
    LightData3D Light;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
ParametricInstances3D g_Instances;

[[vk::binding(ONYX_BOUNDS3_BINDING, 0)]]
Bounds3D g_Bounds;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const ParametricInstanceData3D pdata = g_Instances[instanceIndex];
    const InstanceData3D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v3 pos = GetParametricPosition3D(input.Position, input.Normal, params, input.Region);

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v3 alignment = ComputeAlignment3D(data.Alignment, pdata.BoundsHandle, g_Bounds);
    const f32v4 worldPos = mul(f32v4(pos - alignment, 1.0), transform);

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
    output.FragColor = unpackUnorm4x8ToFloat(data.FillColor);

    return output;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

[[vk::binding(ONYX_MATERIALS3_BINDING, 0)]]
Materials3D g_Materials;

[[vk::binding(ONYX_PLIGHTS2_BINDING, 0)]]
PointLights3D g_PointLights;

[[vk::binding(ONYX_DLIGHTS_BINDING, 0)]]
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

