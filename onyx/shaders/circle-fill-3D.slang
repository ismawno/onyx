#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    nointerpolation float3 FragNormal;
    [[vk::location(1)]]
    float2 LocalPosition;
    [[vk::location(2)]]
    float3 WorldPosition;
    [[vk::location(3)]]
    nointerpolation ArcData Arc;
    [[vk::location(9)]]
    nointerpolation FadeData Fade;
    [[vk::location(11)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
    float  PointSize : SV_PointSize;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataFill3D, Std430DataLayout> instanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData3D Light;
};

[[vk::push_constant]]
PushConstants pdata;


static const float2 g_Positions[6] =
{
    float2(-0.5, -0.5),
    float2( 0.5,  0.5),
    float2(-0.5,  0.5),
    float2(-0.5, -0.5),
    float2( 0.5, -0.5),
    float2( 0.5,  0.5)
};

[shader("vertex")]
VertexOutput mainVS(
    const uint vertexId : SV_VertexID,
    const uint baseVertex : SV_StartVertexLocation,
    const uint instanceId : SV_InstanceID,
    const uint baseInstance : SV_StartInstanceLocation)
{
    const uint vertexIndex = vertexId + baseVertex;
    const uint instanceIndex = instanceId + baseInstance;

    const CircleInstanceDataFill3D data = instanceBuffer[instanceIndex];
    const float4x4 transform = ComputeTransform3D(data.Data.Basis);

    const float2 localPos = g_Positions[vertexIndex];
    const float4 worldPos = mul(transform, float4(localPos, 0.0, 1.0));

    VertexOutput output;
    output.Position   = mul(worldPos, pdata.ProjectionView);
    output.PointSize  = 1.0;

    output.FragNormal = ComputeTrivialWorldNormal(float3x3(transform));
    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xyz;
    output.Arc = data.Arc;
    output.Fade = data.Fade;
    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

    return output;
}

[[vk::binding(1, 0)]]
StructuredBuffer<PointLight3D, Std430DataLayout> pointLights;

[[vk::binding(2, 0)]]
StructuredBuffer<DirectionalLight, Std430DataLayout> directionalLights;

[shader("fragment")]
float4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    float3 normal = input.FragNormal;
    if (!isFrontFacing)
        normal = -normal;

    const float3 lcolor = GetLightColor3D(pdata.Light, normal, input.WorldPosition, pointLights, directionalLights);
    const float alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    return input.FragColor * float4(lcolor, alpha);
}

