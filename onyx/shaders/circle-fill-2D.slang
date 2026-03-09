#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    float2 LocalPosition;
    [[vk::location(1)]]
    float2 TexCoord;
    [[vk::location(2)]]
    float2 WorldPosition;
    [[vk::location(3)]]
    nointerpolation uint MatIndex;
    [[vk::location(4)]]
    nointerpolation ArcData Arc;
    [[vk::location(7)]]
    nointerpolation float2 Fade;
    [[vk::location(8)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataFill2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData2D Light;
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

static const float2 g_TexCoords[6] =
{
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(0.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0)
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

    const CircleInstanceDataFill2D data = g_Instances[instanceIndex];
    const float4x4 transform = ComputeTransform2D(data.Data.Basis);

    const float2 localPos = g_Positions[vertexIndex];
    const float4 worldPos = mul(float4(localPos, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = g_TexCoords[vertexIndex];

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xy;
    output.MatIndex = data.Data.MatIndex;

    output.Arc.CosSin = float4(data.Arc.LowerCos, data.Arc.LowerSin, data.Arc.UpperCos, data.Arc.UpperSin);
    output.Arc.AngleOverflow = data.Arc.AngleOverflow;
    output.Arc.Hollowness = data.Arc.Hollowness;

    output.Fade = float2(data.Fade.InnerFade, data.Fade.OuterFade);
    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

    return output;
}

[[vk::binding(1, 0)]]
Materials2D g_Materials;

[[vk::binding(2, 0)]]
SamplerState g_Samplers[8];

[[vk::binding(3, 0)]]
Texture2D g_Textures[1024];

[[vk::binding(4, 0)]]
PointLights2D g_PointLights;

[shader("fragment")]
float4 mainFS(const VertexOutput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;


    const float3 lcolor = GetLightColor2D(g_PushData.Light, input.WorldPosition, g_PointLights);
    const float alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    if (input.MatIndex != uint(-1))
    {
        const Material2D mat = g_Materials[input.MatIndex];
        if (mat.Texture == uint(-1) || mat.Sampler == uint(-1))
            return input.FragColor * unpackUnorm4x8ToFloat(g_Materials[input.MatIndex].ColorFactor) * float4(lcolor, alpha);

        const float4 tex = g_Textures[mat.Texture].Sample(g_Samplers[mat.Sampler], input.TexCoord);
        return tex * input.FragColor * unpackUnorm4x8ToFloat(g_Materials[input.MatIndex].ColorFactor) * float4(lcolor, alpha);
    }
    return input.FragColor * float4(lcolor, alpha);
}
