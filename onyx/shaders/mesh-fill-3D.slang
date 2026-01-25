#include "utils.slang"

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

struct VertexOutput
{
    [[vk::location(0)]]
    float3 FragNormal;

    [[vk::location(1)]]
    float3 WorldPosition;

    [[vk::location(2)]]
    nointerpolation MaterialData3D Material;

    float4 Position : SV_Position;
    float  PointSize : SV_PointSize;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceDataFill3D, Std430DataLayout> instanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData3D LightData;
};

[[vk::push_constant]]
PushConstants pdata;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const uint instanceId : SV_InstanceID,
    const uint baseInstance : SV_StartInstanceLocation)
{
    const uint instanceIndex = instanceId + baseInstance;

    const InstanceDataFill3D data = instanceBuffer[instanceIndex];

    const float4x4 transform = ComputeTransform3D(data.Basis);
    const float4 worldPos = mul(transform, float4(input.Position, 1.0));

    VertexOutput output;
    output.Position = mul(worldPos, pdata.ProjectionView);
    output.PointSize = 1.0;

    const float3x3 normalMatrix = ComputeNormalMatrix(float3x3(transform));

    output.FragNormal = normalize(mul(normalMatrix, input.Normal));
    output.WorldPosition = worldPos.xyz;
    output.Material.Color = unpackUnorm4x8ToFloat(data.Color);
    output.Material.Reflection = data.Reflection;

    return output;
}

[[vk::binding(0, 1)]]
StructuredBuffer<DirectionalLight, Std430DataLayout> directionalLights;
[[vk::binding(1, 1)]]
StructuredBuffer<PointLight, Std430DataLayout> pointLights;


[shader("fragment")]
float4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
{
    float3 normal = normalize(input.FragNormal);
    if (!isFrontFacing)
        normal = -normal;

    const LightData3D ldata = pdata.LightData;
    const float3 color = GetLightColor(ldata, normal, input.Material.Reflection, input.WorldPosition, directionalLights, pointLights);

    return min(input.Material.Color * float4(color, 1.0), 1.0);
}

