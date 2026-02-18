#include "utils.slang"

struct VertexInput
{
    float2 Position : POSITION;
};

struct VertexOutput
{
    [[vk::location(0)]]
    float2 WorldPosition;
    [[vk::location(1)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
    float  PointSize : SV_PointSize;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceData2D, Std430DataLayout> instanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData2D Light;
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

    const InstanceData2D data = instanceBuffer[instanceIndex];

    const float4x4 transform = ComputeTransform2D(data.Basis);
    const float4 worldPos = mul(transform, float4(input.Position, 0.0, 1.0));
    VertexOutput output;
    output.Position =  mul(worldPos, pdata.ProjectionView);
    output.PointSize = 1.0;

    output.WorldPosition = worldPos.xy;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

    return output;
}

[[vk::binding(1, 0)]]
StructuredBuffer<PointLight2D, Std430DataLayout> pointLights;

[shader("fragment")]
float4 mainFS(const VertexOutput input)
{
    const float3 lcolor = GetLightColor2D(pdata.Light, input.WorldPosition, pointLights);
    return input.FragColor * float4(lcolor, 1.f);
}
