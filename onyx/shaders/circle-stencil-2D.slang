#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    float2 LocalPosition;
    [[vk::location(1)]]
    nointerpolation ArcData Arc;
    [[vk::location(4)]]
    nointerpolation float2 Fade;
    [[vk::location(5)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataStencil2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    float4x4 ProjectionView;
};

[[vk::push_constant]]
PushConstants g_PushData;

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

    const CircleInstanceDataStencil2D data = g_Instances[instanceIndex];
    const float4x4 transform = ComputeTransform2D(data.Data.Basis);

    const float2 localPos = g_Positions[vertexIndex];
    const float2 scale = ComputeStencilScale2D(transform);
    const float2 extruded = localPos + (normalize(localPos) * data.Data.OutlineWidth) / scale;

    const float4 pos = float4(extruded, 0.0, 1.0);

    VertexOutput output;
    output.Position = mul(mul(pos, transform), g_PushData.ProjectionView);

    output.LocalPosition = localPos;

    output.Arc.CosSin = float4(data.Arc.LowerCos, data.Arc.LowerSin, data.Arc.UpperCos, data.Arc.UpperSin);
    output.Arc.AngleOverflow = data.Arc.AngleOverflow;
    output.Arc.Hollowness = data.Arc.Hollowness;

    output.Fade = float2(data.Fade.InnerFade, data.Fade.OuterFade);

    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

    return output;
}

[shader("fragment")]
float4 mainFS(const VertexOutput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

    float4 color = input.FragColor;
    color.a = CircleFade(color.a, bounds, input.Fade);
    return color;
}
