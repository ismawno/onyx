#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    nointerpolation float4 FragColor;
    [[vk::location(1)]]
    float2 LocalPosition;
    [[vk::location(2)]]
    nointerpolation ArcData Arc;
    [[vk::location(8)]]
    nointerpolation FadeData Fade;

    float4 Position : SV_Position;
    float  PointSize : SV_PointSize;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceData2D, Std430DataLayout> instanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
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

    CircleInstanceData2D data = instanceBuffer[instanceIndex];
    float4x4 transform = ComputeTransform2D(data.Data.Basis);

    float2 localPos = g_Positions[vertexIndex];
    float4 pos = float4(localPos, 0.0, 1.0);

    VertexOutput output;
    output.Position   = mul(mul(transform, pos), pdata.ProjectionView);
    output.PointSize  = 1.0;

    output.FragColor     = unpackUnorm4x8ToFloat(data.Data.Color);
    output.LocalPosition = localPos;
    output.Arc           = data.Arc;
    output.Fade          = data.Fade;

    return output;
}

[shader("fragment")]
float4 mainFS(const VertexOutput p_Input)
{
    const CircleBounds bounds = GetCircleBounds(p_Input.LocalPosition, p_Input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, p_Input.LocalPosition, p_Input.Arc)) discard;

    float4 color = p_Input.FragColor;
    color.a = CircleFade(color.a, bounds, p_Input.Fade);
    return color;
}
